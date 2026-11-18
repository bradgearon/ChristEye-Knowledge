#include "anaiero/application/environment_calibration_application.hpp"
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
namespace fs=std::filesystem;
namespace c=anaiero::science::calibration;
namespace db=christeye::living::durable;
static unsigned checks=0;
#define CHECK(value) do {++checks;if(!(value)){std::cerr<<"check failed at "<<__LINE__<<": "<<#value<<'\n';std::exit(1);}}while(false)
static std::string hash(std::string_view bytes){return christeye::living::plans::sha256_hex(bytes);}
static void write(const fs::path& path,std::string_view text){
  std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write(text.data(),static_cast<std::streamsize>(text.size()));file.close();CHECK(file.good());
}
static std::string read(const fs::path& path){std::string bytes;CHECK(db::read_bounded(path,8*1024*1024,bytes));return bytes;}
struct CountingTrial final:c::IEnvironmentTrial {
  c::NativeEnvironmentTrial delegate;
  std::vector<std::uint64_t> starts;
  std::uint64_t advances=0,fail_after=UINT64_MAX;
  std::string_view behavior_sha256()const noexcept override{return delegate.behavior_sha256();}
  bool begin(const anaiero::science::EnvironmentCompartmentModel& model,std::uint64_t step)noexcept override{
    starts.push_back(model.exchange_tau_ns);return delegate.begin(model,step);
  }
  bool advance(std::uint64_t sequence,std::int32_t input,std::int64_t& value)noexcept override{
    if(advances==fail_after)return false;
    ++advances;return delegate.advance(sequence,input,value);
  }
};
struct FlagCancellation final:c::ICancellation {
  std::atomic_bool flag{false};
  bool cancelled()const noexcept override{return flag.load();}
};
struct CountdownCancellation final:c::ICancellation {
  mutable unsigned remaining=5;
  bool cancelled()const noexcept override{return remaining?--remaining==0:true;}
};
struct Observer final:c::IObserver {
  CountingTrial& trial;
  FlagCancellation* cancellation=nullptr;
  std::vector<c::Phase> phases;
  std::size_t starts_at_freeze=0;
  explicit Observer(CountingTrial& value):trial(value){}
  void on_calibration_event(const c::Event& event)noexcept override{
    phases.push_back(event.phase);
    if(event.phase==c::Phase::winner_frozen){starts_at_freeze=trial.starts.size();if(cancellation)cancellation->flag.store(true);}
  }
};
static void repin(c::Plan& plan,c::Reference& reference,bool holdout){
  c::files::ReferenceFile record{reference,{"unit-test.raw",reference.provenance.raw_sha256}};
  reference.sha256=hash(c::files::serialize_reference(record));
  if(holdout)plan.holdout_sha256=reference.sha256;else plan.training_sha256=reference.sha256;
  plan.sha256=hash(plan.name+plan.training_sha256+plan.holdout_sha256);
}
static c::Report run(const c::Plan& plan,const c::Reference& training,const c::Reference& holdout,c::Code expected=c::Code::ok){
  c::NativeEnvironmentTrial trial;c::AbsoluteFidelityPolicy policy;c::EnvironmentCalibrationService service(trial,policy);
  c::Report report;CHECK(service.run(plan,training,holdout,report)==expected);CHECK(report.code==expected);return report;
}
static void replace_reference(c::Plan& plan,c::Reference& reference,const c::Candidate& candidate,bool holdout){
  anaiero::science::EnvironmentCompartmentService model;CHECK(model.initialize(candidate.model,reference.step_ns));
  for(auto& sample:reference.samples){CHECK(model.advance(sample.sequence,sample.inlet_input));sample.concentration_micromolar=model.concentration_micromolar();}
  reference.provenance.raw_sha256=hash(reference.provenance.raw_sha256+candidate.id);repin(plan,reference,holdout);
}
static void selection_and_failure(const c::Plan& plan,const c::Reference& training,const c::Reference& holdout){
  CountingTrial trial;c::AbsoluteFidelityPolicy policy;c::EnvironmentCalibrationService service(trial,policy);Observer observer(trial);
  service.set_observer(&observer);c::Report report;
  CHECK(service.run(plan,training,holdout,report)==c::Code::ok);
  CHECK(report.passed&&report.training_passed&&report.holdout_passed&&report.winner_frozen);
  CHECK(report.selected_index==1);CHECK(report.frozen_candidate_id=="tau-1000ms");
  CHECK(report.frozen_model_sha256==plan.candidates[1].model_sha256);
  CHECK(observer.starts_at_freeze==plan.candidates.size());CHECK(trial.starts.size()==plan.candidates.size()+1);
  CHECK(trial.starts.back()==plan.candidates[1].model.exchange_tau_ns);
  CHECK(trial.advances==training.samples.size()*plan.candidates.size()+holdout.samples.size());
  CHECK(report.candidates[1].training.sum_squared_error==0&&report.holdout.sum_squared_error==0);
  CHECK(report.holdout.max_reference_uncertainty_micromolar==1);
  CHECK(report.step_ns==training.step_ns&&report.initial_micromolar==training.initial_micromolar);
  CHECK(observer.phases.front()==c::Phase::validated&&observer.phases.back()==c::Phase::holdout_complete);
  const auto json=c::files::serialize_report(report);
  CHECK(json.find("\"biological_validation\":false")!=json.npos);
  CHECK(json.find("\"scientific_optimization_enabled\":false")!=json.npos);
  CHECK(json.find("\"measurement_authenticity_verified\":false")!=json.npos);
  CHECK(json.find("NUMERICAL_REFERENCE_COMPARISON")!=json.npos);
  CHECK(json.find("\"holdout_evaluations\":1")!=json.npos);
  CHECK(json.find("\"uncertainty_relaxes_threshold\":false")!=json.npos);

  auto altered_plan=plan;auto altered_holdout=holdout;
  replace_reference(altered_plan,altered_holdout,plan.candidates[2],true);
  CountingTrial changed;c::EnvironmentCalibrationService selected(changed,policy);c::Report failed;
  CHECK(selected.run(altered_plan,training,altered_holdout,failed)==c::Code::ok);
  CHECK(failed.selected_index==1&&failed.winner_frozen&&failed.training_passed&&!failed.holdout_passed&&!failed.passed);
  CHECK(changed.starts.size()==plan.candidates.size()+1&&changed.starts.back()==plan.candidates[1].model.exchange_tau_ns);
  CHECK(failed.holdout.max_absolute_error_micromolar>plan.criteria.max_absolute_error_micromolar);
  // A declared large uncertainty cannot wash out a bad model fit.
  for(auto& sample:altered_holdout.samples)sample.uncertainty_micromolar=1000000;
  repin(altered_plan,altered_holdout,true);
  CHECK(!run(altered_plan,training,altered_holdout).passed);

  // Passing holdout alone cannot hide a failed training fit.
  altered_plan=plan;altered_plan.candidates={plan.candidates[1]};auto altered_training=training;
  for(auto& sample:altered_training.samples)sample.concentration_micromolar+=50;
  repin(altered_plan,altered_training,false);
  const auto poor_training=run(altered_plan,altered_training,holdout);
  CHECK(!poor_training.training_passed&&poor_training.holdout_passed&&!poor_training.passed);

  auto external_plan=plan;auto external_training=training;auto external_holdout=holdout;
  external_training.provenance.origin=c::ReferenceOrigin::external_unverified_readonly;
  external_holdout.provenance.origin=c::ReferenceOrigin::external_unverified_readonly;
  repin(external_plan,external_training,false);repin(external_plan,external_holdout,true);
  const auto declared=run(external_plan,external_training,external_holdout);CHECK(declared.passed);
  const auto declared_json=c::files::serialize_report(declared);
  CHECK(declared_json.find("DECLARED_EXTERNAL_REFERENCE_COMPARISON_UNVERIFIED")!=declared_json.npos);
  CHECK(declared_json.find("\"measurement_authenticity_verified\":false")!=declared_json.npos);

  c::ErrorSummary boundary{2,25,4,1000};
  CHECK(!policy.accepts(boundary,{4,3}));CHECK(policy.accepts(boundary,{4,4}));CHECK(!policy.accepts(boundary,{3,4}));
}
static void validation(const c::Plan& plan,const c::Reference& training,const c::Reference& holdout){
  auto reference_failure=[&](auto change,c::Code expected=c::Code::invalid_reference){
    auto p=plan;auto data=holdout;change(data);repin(p,data,true);run(p,training,data,expected);
  };
  reference_failure([](auto& r){r.provenance.source=0;});
  reference_failure([](auto& r){r.provenance.epoch=0;});
  reference_failure([](auto& r){r.provenance.clock_domain=0;});
  reference_failure([](auto& r){r.provenance.run_id="bad run";});
  reference_failure([](auto& r){r.provenance.producer_sha256="invalid";});
  reference_failure([](auto& r){r.provenance.protocol_sha256=hash("different-protocol");});
  reference_failure([](auto& r){r.unit=anaiero::science::Unit::dimensionless_proxy;});
  reference_failure([](auto& r){r.step_ns=0;});
  reference_failure([](auto& r){r.step_ns=1000000001;});
  reference_failure([](auto& r){r.initial_micromolar+=1;});
  reference_failure([](auto& r){r.initial_uncertainty_micromolar=-1;});
  reference_failure([](auto& r){r.samples[1].sequence=10;});
  reference_failure([](auto& r){r.samples[1].acquired_ns=1;});
  reference_failure([](auto& r){r.samples[1].inlet_input=1001;});
  reference_failure([](auto& r){r.samples[1].concentration_micromolar=-1;});
  reference_failure([](auto& r){r.samples[1].concentration_micromolar=1000001;});
  reference_failure([](auto& r){r.samples[1].uncertainty_micromolar=-1;});
  reference_failure([](auto& r){r.samples.clear();});
  reference_failure([](auto& r){r.samples.resize(c::kMaxSamples+1);});
  reference_failure([&](auto& r){r.provenance.source=training.provenance.source;r.provenance.epoch=training.provenance.epoch;},c::Code::contaminated_split);
  reference_failure([&](auto& r){r.provenance.run_id=training.provenance.run_id;},c::Code::contaminated_split);
  reference_failure([&](auto& r){r.provenance.raw_sha256=training.provenance.raw_sha256;},c::Code::contaminated_split);
  reference_failure([](auto& r){r.provenance.origin=c::ReferenceOrigin::external_unverified_readonly;});
  auto same_plan=plan;same_plan.holdout_sha256=training.sha256;
  run(same_plan,training,training,c::Code::contaminated_split);
  auto wrong_hash=plan;wrong_hash.holdout_sha256=hash("unrelated-file");run(wrong_hash,training,holdout,c::Code::invalid_reference);
  auto plan_failure=[&](auto change){auto p=plan;change(p);run(p,training,holdout,c::Code::invalid_plan);};
  plan_failure([](auto& p){p.candidates.clear();});
  plan_failure([](auto& p){p.candidates.resize(c::kMaxCandidates+1);});
  plan_failure([](auto& p){p.criteria.max_rmse_micromolar=1000001;});
  plan_failure([](auto& p){p.criteria.max_absolute_error_micromolar=1000001;});
  plan_failure([](auto& p){p.candidates[0].model.exchange_tau_ns=0;});
  plan_failure([](auto& p){p.candidates[0].model.initial_micromolar+=1;});
  plan_failure([](auto& p){p.candidates[0].model_sha256="invalid";});
  plan_failure([](auto& p){p.candidates[0].id=p.candidates[1].id;});
  plan_failure([](auto& p){p.candidates[0].model_sha256=p.candidates[1].model_sha256;});
  plan_failure([](auto& p){p.candidates[0].model=p.candidates[1].model;});
}
static void lifecycle(const c::Plan& plan,const c::Reference& training,const c::Reference& holdout){
  c::AbsoluteFidelityPolicy policy;CountingTrial trial;c::EnvironmentCalibrationService service(trial,policy);c::Report report;
  FlagCancellation cancelled;cancelled.flag.store(true);
  CHECK(service.run(plan,training,holdout,report,&cancelled)==c::Code::cancelled);
  CHECK(trial.starts.empty()&&!report.winner_frozen&&!report.passed);
  CHECK(c::files::serialize_report(report).empty());
  CountdownCancellation budget;
  CHECK(service.run(plan,training,holdout,report,&budget)==c::Code::cancelled);
  CHECK(trial.advances<=5&&!report.winner_frozen);
  CountingTrial frozen;c::EnvironmentCalibrationService freeze_service(frozen,policy);Observer freeze_observer(frozen);FlagCancellation at_freeze;
  freeze_observer.cancellation=&at_freeze;freeze_service.set_observer(&freeze_observer);
  CHECK(freeze_service.run(plan,training,holdout,report,&at_freeze)==c::Code::cancelled);
  CHECK(report.winner_frozen&&frozen.starts.size()==plan.candidates.size()&&!report.passed);
  CHECK(c::files::serialize_report(report).empty());
  CountingTrial broken;broken.fail_after=3;c::EnvironmentCalibrationService broken_service(broken,policy);
  CHECK(broken_service.run(plan,training,holdout,report)==c::Code::model_failure);
  CHECK(!report.passed&&!report.winner_frozen&&broken.advances==3);
  struct Reentrant final:c::IObserver {
    c::EnvironmentCalibrationService& owner;const c::Plan& p;const c::Reference& train;const c::Reference& hold;
    bool rejected=false;
    Reentrant(c::EnvironmentCalibrationService& service_value,const c::Plan& plan_value,const c::Reference& training_value,const c::Reference& holdout_value):
      owner(service_value),p(plan_value),train(training_value),hold(holdout_value){}
    void on_calibration_event(const c::Event& event)noexcept override{
      if(event.phase==c::Phase::validated){c::Report other;rejected=owner.run(p,train,hold,other)==c::Code::reentrant;}
    }
  } reentrant(service,plan,training,holdout);
  service.set_observer(&reentrant);CHECK(service.run(plan,training,holdout,report)==c::Code::ok&&report.passed);CHECK(reentrant.rejected);
}
static void bounded_execution(const c::Plan& plan,const c::Reference& training,const c::Reference& holdout){
  auto p=plan;p.candidates={plan.candidates[1]};auto train=training;auto hold=holdout;
  auto extend=[&](c::Reference& reference,bool held){
    const auto original=reference.samples.size();reference.samples.resize(c::kMaxSamples);
    for(std::size_t i=original;i<reference.samples.size();++i){
      auto& sample=reference.samples[i];sample.sequence=static_cast<std::uint64_t>(i)+1;
      sample.acquired_ns=sample.sequence*reference.step_ns;sample.inlet_input=500;sample.uncertainty_micromolar=1;
    }
    replace_reference(p,reference,p.candidates[0],held);
  };
  extend(train,false);extend(hold,true);
  const auto report=run(p,train,hold);CHECK(report.passed&&report.holdout.count==4096&&report.candidates[0].training.count==4096);
}
static void file_boundaries(const fs::path& fixture,const fs::path& temporary){
  const auto plan_path=fixture/"reference.cevcalibration";
  const auto original_plan=read(plan_path);c::files::PlanFiles p;
  CHECK(c::files::parse_plan(original_plan,p));CHECK(c::files::serialize_plan(p)==original_plan);
  CHECK(!c::files::parse_plan(original_plan+"\n",p));
  auto damaged=original_plan;damaged[10]='x';CHECK(!c::files::parse_plan(damaged,p));
  const auto reference_path=fixture/"training.cevreference";const auto original_reference=read(reference_path);c::files::ReferenceFile record;
  CHECK(c::files::parse_reference(original_reference,record));CHECK(c::files::serialize_reference(record)==original_reference);
  auto with_bad_unit=record;with_bad_unit.reference.unit=anaiero::science::Unit::millimolar;
  CHECK(!c::files::parse_reference(c::files::serialize_reference(with_bad_unit),with_bad_unit));
  auto noncanonical=original_reference;const auto start=noncanonical.find("source 901\n");CHECK(start!=noncanonical.npos);
  noncanonical.replace(start,11,"source 0901\n");std::string_view body;
  noncanonical=c::files::seal(noncanonical.substr(0,noncanonical.size()-70));
  CHECK(!c::files::parse_reference(noncanonical,record));
  const auto copy=temporary/"fixture";fs::copy(fixture,copy,fs::copy_options::recursive);
  c::Plan plan;c::Reference train,hold;std::string error;
  const auto raw_path=copy/"training.raw.csv";const auto raw=read(raw_path);
  write(raw_path,raw+"tampered\n");CHECK(!c::files::read_bundle(copy/"reference.cevcalibration",plan,train,hold,error));
  CHECK(error.find("raw reference artifact")!=error.npos);write(raw_path,raw);
  const auto model_path=copy/"tau-1000ms.cevmodel";const auto model=read(model_path);
  write(model_path,model+"\n");CHECK(!c::files::read_bundle(copy/"reference.cevcalibration",plan,train,hold,error));
  CHECK(error.find("candidate model")!=error.npos);write(model_path,model);
  CHECK(c::files::read_bundle(copy/"reference.cevcalibration",plan,train,hold,error));
  CHECK(c::files::parse_reference(original_reference,record));record.reference.samples[2].acquired_ns=1;
  const auto bad_reference=c::files::serialize_reference(record);write(copy/"training.cevreference",bad_reference);
  CHECK(c::files::parse_plan(original_plan,p));p.training.sha256=hash(bad_reference);write(copy/"reference.cevcalibration",c::files::serialize_plan(p));
  const auto invalid=anaiero::application::calibrate_environment(copy/"reference.cevcalibration",temporary/"invalid.json");
  CHECK(invalid.exit_code==2&&!fs::exists(temporary/"invalid.json"));
  write(copy/"training.cevreference",original_reference);write(copy/"reference.cevcalibration",original_plan);
  write(raw_path,std::string(c::files::kMaxRawBytes+1,'x'));
  CHECK(!c::files::read_bundle(copy/"reference.cevcalibration",plan,train,hold,error));write(raw_path,raw);

  const auto valid=anaiero::application::calibrate_environment(plan_path,temporary/"qualification.json");
  CHECK(valid.exit_code==0&&valid.error.empty()&&valid.report.passed);
  const auto first=read(temporary/"qualification.json");
  const auto repeated=anaiero::application::calibrate_environment(plan_path,temporary/"qualification.json");
  CHECK(repeated.exit_code==2&&read(temporary/"qualification.json")==first);
  const auto pending=temporary/"preserved.json.pending";write(pending,"existing staging belongs to someone else");
  CHECK(!c::files::write_new_report(temporary/"preserved.json","{\"value\":1}\n",error));
  CHECK(read(pending)=="existing staging belongs to someone else"&&!fs::exists(temporary/"preserved.json"));
  const auto protected_file=temporary/"protected.txt";write(protected_file,"protected hard-link bytes");
  fs::create_hard_link(protected_file,temporary/"hardlink.json.pending");
  CHECK(!c::files::write_new_report(temporary/"hardlink.json","{\"value\":2}\n",error));
  CHECK(read(protected_file)=="protected hard-link bytes");
  db::os::File held_lock;CHECK(held_lock.open(temporary/"locked.json.lock")&&held_lock.lock_exclusive());
  CHECK(!c::files::write_new_report(temporary/"locked.json","{}\n",error)&&!fs::exists(temporary/"locked.json"));
  held_lock.close();
  std::atomic<unsigned> winners{0};
  const auto race=temporary/"concurrent.json";
  std::thread a([&]{std::string failure;if(c::files::write_new_report(race,"{\"winner\":1}\n",failure))++winners;});
  std::thread b([&]{std::string failure;if(c::files::write_new_report(race,"{\"winner\":2}\n",failure))++winners;});
  a.join();b.join();CHECK(winners.load()==1);const auto race_bytes=read(race);
  CHECK(race_bytes=="{\"winner\":1}\n"||race_bytes=="{\"winner\":2}\n");
  CHECK(!fs::exists(temporary/"concurrent.json.pending"));
}
int main(int argc,char** argv){
  if(argc!=3)return 2;
  const auto fixture=fs::path(argv[1])/"examples/scientific/calibration";
  const auto temporary=fs::absolute(argv[2]);fs::remove_all(temporary);fs::create_directories(temporary);
  c::Plan plan;c::Reference training,holdout;std::string error;
  CHECK(c::files::read_bundle(fixture/"reference.cevcalibration",plan,training,holdout,error));
  selection_and_failure(plan,training,holdout);validation(plan,training,holdout);
  lifecycle(plan,training,holdout);bounded_execution(plan,training,holdout);file_boundaries(fixture,temporary);
  std::cout<<"scientific calibration: "<<checks<<" checks passed\n";return 0;
}

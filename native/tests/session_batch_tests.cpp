// Full native session composition: durable operations, real bounded experiments,
// staged live outcomes, segmented evidence and checkpoint/tail continuation.
#include "anaiero/application/native_session_application.hpp"
#include "anaiero/infrastructure/experiment_files.hpp"
#include "anaiero/infrastructure/observation_checkpoint_codec.hpp"
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x <<" line "<<__LINE__<<'\n';std::abort();}}while(false)
#include "session_science_batch.hpp"
namespace app=anaiero::application;
namespace session=anaiero::session;
namespace op=anaiero::operations;
namespace ex=anaiero::experiments;
namespace eng=anaiero::engine;
namespace runtime=anaiero::runtime;
namespace db=christeye::living::durable;
namespace fs=std::filesystem;
static void write_file(const fs::path& path,std::string_view bytes){
  fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out<<bytes;CHECK(out.good());
}
static fs::path configuration(const fs::path& repo,const fs::path& root,std::string_view name){
  const auto path=root/(std::string(name)+".cevsession");std::ostringstream out;
  out<<"CEVSESSION/2\nname "<<name<<"\nperiod_ms 10\nstorage segmented 64\n"
    <<"engine 101 sw5 61 1 1 "<<std::quoted((repo/"examples/cells/crest-blood-neural.cevatlas").string())<<' '
    <<std::quoted((repo/"examples/runtime/two-neurons.cevcells").string())<<" 75 0 64 1\n"
    <<"engine 201 sim3 71 1 2 "<<std::quoted((repo/"examples/connected-gardens.cev").string())<<" \"-\" 0 0 0 0\nend\n";
  write_file(path,out.str());return path;
}
static std::unique_ptr<app::NativeSessionApplication> open_session(const fs::path& config,const fs::path& root){
  auto value=std::make_unique<app::NativeSessionApplication>();const bool opened=value->open(config,root);
  if(!opened)std::cerr<<"open: "<<value->error()<<'\n';
  CHECK(opened);return value;
}
static void settle(app::NativeSessionApplication& value){
  const bool ready=value.settle(std::chrono::seconds(240));
  if(!ready)std::cerr<<value.driver().state_json();
  CHECK(ready&&!value.driver().busy()&&!value.driver().failed()&&value.driver().quiescent());
}
static session::Command command(session::Operation operation,std::uint64_t id,std::string reference={}){
  session::Command result{};result.operation=operation;result.operation_id=id;result.reference=std::move(reference);return result;
}
static op::Entry receipt(const session::IDriver& driver,std::uint64_t id){
  const auto view=driver.operation_view();
  for(std::size_t i=0;i<view.count;++i)if(view.entries[i].id==id)return view.entries[i];
  CHECK(false);return {};
}
static bool same_snapshot(const eng::Snapshot& a,const eng::Snapshot& b){
  return a.id==b.id&&a.source_epoch==b.source_epoch&&a.tick==b.tick&&a.revision==b.revision&&
    a.participants==b.participants&&a.connections==b.connections&&a.energy==b.energy&&a.learning_proxy==b.learning_proxy&&a.state_sha256==b.state_sha256;
}
static void visible(const session::View& view){
  CHECK(view.engine_count==2&&view.pending_evidence==0&&view.projected.sources==2&&view.projected.count==10&&view.projected.sequence_gaps==0);
  for(std::size_t i=0;i<view.engine_count;++i){const auto& e=view.engines[i];
    CHECK(same_snapshot(e.working,e.published)&&e.coordinator_revision==e.working.revision);
    CHECK(e.capabilities.clock_domain==e.working.id&&e.capabilities.tick_ns==0&&e.capabilities.evaluate_candidates);
    unsigned seen=0;
    for(std::size_t j=0;j<view.projected.count;++j){const auto& metric=view.projected.values[j];
      if(metric.colony!=e.working.id)continue;
      CHECK(metric.source==e.source&&metric.source_epoch==e.working.source_epoch&&metric.acquired_ns==0);
      switch(metric.metric){
        case runtime::Metric::viability:CHECK(metric.value==static_cast<std::int64_t>(e.working.participants));break;
        case runtime::Metric::connection:CHECK(metric.value==static_cast<std::int64_t>(e.working.connections));break;
        case runtime::Metric::energy:CHECK(metric.value==e.working.energy);break;
        case runtime::Metric::digital_learning:CHECK(metric.value==e.working.learning_proxy);break;
        case runtime::Metric::digital_route_active:CHECK(metric.value==(e.route_active?1:0));break;
        default:CHECK(false);
      }
      ++seen;
    }
    CHECK(seen==5);
  }
}
static ex::Record read_experiment(const fs::path& root,std::uint64_t id,std::uint64_t* events=nullptr){
  db::Registry registry;CHECK(registry.open(root/"session")==db::Code::ok);
  ex::files::WindStoreExperimentStore store(registry);ex::Record result{};bool found=false;
  const auto run="experiment-"+std::to_string(id);CHECK(store.load(run,result,found)&&found);
  if(events){*events=0;CHECK(registry.visit_events(0,[&](const db::Event& event){
    if(event.kind=="MODEL_EVALUATION"&&event.entity==run&&event.clock=="experiment")++*events;
    return true;}));}
  return result;
}
static void experiments_and_results(const fs::path& repo,const fs::path& root){
  const auto config=configuration(repo,root,"experiment-session");const auto state=root/"state";
  ex::Definition plan{};std::string error;
  CHECK(ex::files::load_definition(repo/"examples/experiments/sw5-guidance.cevexperiment",plan,error));
  auto run=command(session::Operation::experiment,10,(repo/"examples/experiments/sw5-guidance.cevexperiment").string());
  std::uint64_t revision=0;std::string first_receipt;
  {
    auto value=open_session(config,state);settle(*value);visible(value->driver().view());
    CHECK(value->driver().execute(run,nullptr));CHECK(value->driver().pump());
    CHECK(value->driver().view().completed_trials==1&&value->driver().view().experiment_state=="running");
    CHECK(value->driver().view().cycles==0&&receipt(value->driver(),10).state==op::State::running);
    const auto trace=value->driver().trace_json();
    CHECK(trace.find("\"service\":10")!=std::string::npos&&trace.find("\"operation_id\":10,\"source\":61,\"epoch\":1")!=std::string::npos);
    revision=value->driver().revision();
  }
  {
    const auto saved=read_experiment(state,10);CHECK(saved.next_candidate==1);first_receipt=saved.trials[0].receipt_sha256;
    auto value=open_session(config,state);CHECK(value->driver().revision()==revision);
    CHECK(value->driver().view().completed_trials==1);settle(*value);
    const auto view=value->driver().view();CHECK(view.experiment_state=="completed"&&view.completed_trials==3&&view.total_trials==3);
    CHECK(view.proposal_approved&&view.proposal_candidate==102&&view.cycles==0);visible(view);
    const auto experiment=value->driver().experiment_view();
    CHECK(experiment.id=="experiment-10"&&experiment.definition_sha256==plan.sha256&&experiment.count==3&&experiment.completed==3);
    CHECK(experiment.selection.candidate.id==102&&experiment.trials[0].receipt_sha256==first_receipt);
    const auto frame=session::encode_experiment(experiment);
    CHECK(frame.size()<16*1024&&frame.find("anaiero.experiment/1")!=std::string::npos&&frame.find(first_receipt)!=std::string::npos);
    auto maximum=experiment;maximum.count=ex::kMaxCandidates;maximum.completed=maximum.count;
    for(std::size_t i=3;i<maximum.count;++i)maximum.trials[i]=experiment.trials[i%3];
    CHECK(session::encode_experiment(maximum).size()<16*1024);
    const auto result=receipt(value->driver(),10);CHECK(result.state==op::State::completed&&result.result==op::Result::ok&&result.session_revision==view.revision);
    revision=value->driver().revision();CHECK(value->driver().execute(run,nullptr));
    CHECK(value->driver().revision()==revision&&receipt(value->driver(),10).session_revision==result.session_revision);
    auto conflict=run;conflict.reference="wrong@"+plan.sha256;CHECK(!value->driver().execute(conflict,nullptr));
    auto next=command(session::Operation::experiment,11,plan.name+"@"+plan.sha256);
    CHECK(value->driver().execute(next,nullptr)&&value->driver().pump());
    value->driver().cancel_operations();settle(*value);
    CHECK(value->driver().view().experiment_state=="paused"&&value->driver().view().completed_trials==1);
    CHECK(receipt(value->driver(),11).state==op::State::cancelled&&receipt(value->driver(),11).result==op::Result::cancelled);
  }
  {
    std::uint64_t events=0;const auto saved=read_experiment(state,10,&events);
    CHECK(saved.next_candidate==3&&saved.trials[0].receipt_sha256==first_receipt&&events==5);
    auto value=open_session(config,state);settle(*value);CHECK(value->driver().view().experiment_state=="paused");
    CHECK(value->driver().execute(command(session::Operation::resume_experiment,12),nullptr));settle(*value);
    CHECK(value->driver().view().experiment_state=="completed"&&value->driver().view().proposal_candidate==102);
    CHECK(receipt(value->driver(),12).state==op::State::completed&&receipt(value->driver(),11).state==op::State::cancelled);
    const auto sim3=command(session::Operation::experiment,13,(repo/"examples/experiments/sim3-teaching.cevexperiment").string());
    CHECK(value->driver().execute(sim3,nullptr));settle(*value);
    CHECK(value->driver().view().completed_trials==3&&value->driver().view().experiment_state=="completed");
    CHECK(!value->driver().view().proposal_approved&&value->driver().view().cycles==0);visible(value->driver().view());
    const auto before=value->driver().view();
    const auto invalid=command(session::Operation::experiment,14,"missing@"+plan.sha256);
    CHECK(value->driver().execute(invalid,nullptr));settle(*value);
    CHECK(receipt(value->driver(),14).state==op::State::failed&&receipt(value->driver(),14).result==op::Result::invalid);
    CHECK(!value->driver().failed()&&same_snapshot(before.engines[0].working,value->driver().view().engines[0].working));
    CHECK(value->driver().execute(invalid,nullptr));
    CHECK(receipt(value->driver(),14).state==op::State::failed&&value->driver().view().cycles==0);
  }
  CHECK(read_experiment(state,11).selection.candidate.id==102);
  CHECK(read_experiment(state,13).selection.candidate.id==202);
}
static void automatic_outcome(const fs::path& repo,const fs::path& root){
  const auto config=configuration(repo,root,"automatic-session");const auto state=root/"state";
  ex::Definition plan{};std::string error;
  CHECK(ex::files::load_definition(repo/"examples/experiments/sw5-guidance.cevexperiment",plan,error));
  plan.name="staged-live-outcome";plan.automatic_apply=true;plan.automatic_revert=true;
  plan.stages[0].inputs={0,0,0,false};plan.stages[1].inputs={75,0,64,true};
  CHECK(ex::files::seal_definition(plan));const auto path=root/"staged.cevexperiment";write_file(path,ex::files::serialize_definition(plan));
  {
    auto value=open_session(config,state);settle(*value);
    CHECK(value->driver().execute(command(session::Operation::experiment,1,path.string()),nullptr));
    // Each pump persists exactly one bounded candidate. Live time does not
    // move until the selected policy is committed and its outcome is scheduled.
    for(unsigned i=0;i<3;++i)CHECK(value->driver().pump());
    CHECK(value->driver().view().completed_trials==3&&value->driver().view().cycles==0);
    CHECK(value->driver().pump()); // durable apply intent and outcome schedule, before live time advances
    CHECK(value->driver().view().engines[0].route_active&&value->driver().view().cycles==0);
  }
  CHECK(read_experiment(state,1).outcome.change_id==1);
  {
    auto value=open_session(config,state);
    settle(*value);const auto view=value->driver().view();visible(view);
    CHECK(view.cycles==18&&view.engines[0].route_active&&view.proposal_commit==1);
    CHECK(receipt(value->driver(),1).state==op::State::completed&&receipt(value->driver(),1).result==op::Result::ok);
  }
  const auto outcome=read_experiment(state,1);
  CHECK(outcome.outcome.change_id==1&&outcome.outcome.decision==ex::OutcomeDecision::retain&&outcome.outcome.observed.tick==18);
  CHECK(outcome.outcome.observed.state_sha256==outcome.selection.forecast.trial.state_sha256);
  for(auto residual:outcome.outcome.residuals)CHECK(residual==0);
  {
    auto value=open_session(config,state);settle(*value);CHECK(value->driver().view().engines[0].route_active);
    auto undo=command(session::Operation::revert,2);undo.engine=101;
    CHECK(value->driver().execute(undo,nullptr));settle(*value);
    CHECK(!value->driver().view().engines[0].route_active&&value->driver().view().cycles==18);
    CHECK(receipt(value->driver(),2).state==op::State::completed);visible(value->driver().view());
  }
  // Cancellation after the final receipt still precedes authorization of the
  // live apply intent. Its durable operation result must survive a restart.
  const auto cancelled=root/"cancelled-state";
  {
    auto value=open_session(config,cancelled);settle(*value);
    CHECK(value->driver().execute(command(session::Operation::experiment,1,path.string()),nullptr));
    for(unsigned i=0;i<3;++i)CHECK(value->driver().pump());
    CHECK(value->driver().experiment_view().state==ex::RunState::completed&&value->service().record().experiment_pending);
    value->driver().cancel_operations();settle(*value);
    CHECK(!value->driver().view().engines[0].route_active&&!value->service().record().experiment_pending);
    CHECK(value->driver().view().cycles==0&&receipt(value->driver(),1).state==op::State::cancelled);
  }
  {
    auto value=open_session(config,cancelled);settle(*value);
    CHECK(!value->driver().view().engines[0].route_active&&!value->service().record().experiment_pending&&value->driver().view().cycles==0);
    CHECK(receipt(value->driver(),1).state==op::State::cancelled&&value->driver().experiment_view().outcome.change_id==0);
  }
}
static void sustained_projection(const fs::path& repo,const fs::path& root){
  const auto config=configuration(repo,root,"sustained-session");const auto state=root/"state";
  session::View before{};std::uint64_t evidence_count=0;
  {
    auto value=open_session(config,state);settle(*value);
    auto advance=command(session::Operation::step,1);advance.count=270;
    CHECK(value->driver().execute(advance,nullptr));settle(*value);before=value->driver().view();visible(before);
    CHECK(before.cycles==270&&before.engines[0].working.tick==270&&before.engines[1].working.tick==135);
    evidence_count=value->committed_observations_after_shutdown();CHECK(evidence_count==2035);
  }
  {
    db::Registry evidence;CHECK(evidence.open(state/"evidence")==db::Code::ok&&evidence.is_segmented());
    CHECK(evidence.event_count()==evidence_count&&evidence.events().size()<=64&&evidence.capacity().archived_segments>0);
    db::ReplayCheckpoint checkpoint;std::string bytes;bool found=false;
    CHECK(evidence.load_replay_checkpoint(checkpoint,bytes,found)&&found&&checkpoint.sequence>0&&
      checkpoint.sequence<=evidence_count&&evidence_count-checkpoint.sequence<64);
    runtime::ObservationReplayCheckpoint snapshot;
    CHECK(runtime::ObservationCheckpointCodec::decode(bytes,snapshot,"sustained-session"));
    CHECK(snapshot.count==2&&snapshot.state.sequence_gaps==0&&snapshot.state.revision==checkpoint.sequence);
    std::uint64_t tail=0;CHECK(evidence.visit_events(checkpoint.sequence,[&](const db::Event&){++tail;return true;}));
    CHECK(tail==evidence_count-checkpoint.sequence);
    db::Registry metadata;CHECK(metadata.open(state/"session")==db::Code::ok&&metadata.is_segmented());
    CHECK(metadata.event_count()>4096&&metadata.events().size()<=64);
  }
  {
    auto value=open_session(config,state);settle(*value);const auto recovered=value->driver().view();visible(recovered);
    CHECK(recovered.revision==before.revision&&recovered.cycles==before.cycles&&recovered.projected.revision==before.projected.revision);
    for(std::size_t i=0;i<2;++i)CHECK(same_snapshot(recovered.engines[i].working,before.engines[i].working));
    const auto result=receipt(value->driver(),1);CHECK(result.state==op::State::completed&&result.result==op::Result::ok);
    auto advance=command(session::Operation::step,2);advance.count=2;
    CHECK(value->driver().execute(advance,nullptr));settle(*value);visible(value->driver().view());
    CHECK(value->driver().view().cycles==272&&value->committed_observations_after_shutdown()==evidence_count+15);
  }
}
int main(int argc,char** argv){
  CHECK(argc==4);const auto repo=fs::absolute(argv[1]);const auto root=fs::absolute(argv[3]);
  fs::remove_all(root);fs::create_directories(root);
  experiments_and_results(repo,root/"experiments");
  automatic_outcome(repo,root/"automatic");
  sustained_projection(repo,root/"sustained");
  scientific_session_batch(repo,fs::absolute(argv[2]),root/"scientific");
  std::cout<<"session experiments, idempotent operation results, staged outcomes and sustained projection recovery PASS\n";
}

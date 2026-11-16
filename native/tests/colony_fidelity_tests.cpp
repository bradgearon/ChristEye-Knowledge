#include "christeye/living/model_fidelity.hpp"
#include "christeye/living/durable_registry.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fid=christeye::living::fidelity;
namespace sim=christeye::living::sim3;
namespace pl=christeye::living::plans;
namespace db=christeye::living::durable;
namespace fs=std::filesystem;
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAILED " #x << " at line " <<__LINE__<<'\n';std::exit(1);}}while(false)
static fs::path project;
static fs::path cfg_path(){return project/"examples/connected-gardens.cev";}
static fs::path manifest(){return project/"examples/sim3/baseline.ceveval";}
static std::string read(const fs::path& path){std::string s;REQUIRE(sim::bounded_read(path,fid::kMaxManifestBytes,s));return s;}
static std::string reseal(const std::string& source){
 const auto pos=source.rfind("seal ");REQUIRE(pos!=std::string::npos);
 return source.substr(0,pos)+"seal "+pl::sha256_hex(source.substr(0,pos))+"\n";
}
static void test_validation(const sim::Configuration& cfg){
 fid::Bundle bundle;
 REQUIRE(fid::Bundle::load(cfg,manifest(),project,bundle)==fid::Status::ok);
 REQUIRE(bundle.laws.parameters.size()==9);
 REQUIRE(bundle.dataset.rows.size()==28);
 REQUIRE(bundle.manifest.config_hash==cfg.hash);
 REQUIRE(bundle.dataset.source_hash==cfg.material_sha);
 REQUIRE(bundle.laws.matches(cfg));
 REQUIRE(bundle.laws.version==cfg.laws.version);
 REQUIRE(bundle.dataset.sha256==bundle.manifest.obs_sha256);
 auto corrupted=read(project/"examples/sim3/synthetic-reference.cevobs");
 const auto pos=corrupted.find("row 3 all energy");REQUIRE(pos!=std::string::npos);
 corrupted[pos+4]='4';
 fid::Dataset invalid;
 REQUIRE(fid::Dataset::parse(corrupted,invalid).code==fid::Status::invalid_seal);
 corrupted=reseal(corrupted);
 REQUIRE(fid::Dataset::parse(corrupted,invalid).code==fid::Status::bad_dataset);
 auto laws=read(project/"examples/sim3/baseline.celaw");
 const auto at=laws.find("parameter fed_gain 1");REQUIRE(at!=std::string::npos);
 laws.replace(at,20,"parameter fed_gain 2");
 laws=reseal(laws);
 fid::LawProfile changed;
 REQUIRE(fid::LawProfile::parse(laws,changed).code==fid::Status::ok);
 REQUIRE(!changed.matches(cfg));
 laws=read(project/"examples/sim3/baseline.celaw");
 laws.replace(laws.find("parameter blood_step"),21,"parameter bogus_data");
 laws=reseal(laws);
 REQUIRE(fid::LawProfile::parse(laws,changed).code==fid::Status::invalid_format);
 auto plan=read(manifest());
 const auto path_at=plan.find("examples/sim3/baseline.celaw");
 REQUIRE(path_at!=std::string::npos);
 plan.replace(path_at,29,"../evil.celaw");
 plan=reseal(plan);
 fid::Manifest bad;
 REQUIRE(fid::Manifest::parse(plan,bad).code==fid::Status::invalid_format);
}
static void test_execution_and_integrity(const sim::Configuration& cfg,const fs::path& scratch){
 sim::Runner runner;
 REQUIRE(runner.open(cfg_path(),scratch/"run")==sim::Result::ok);
 for(int i=0;i<6;++i)REQUIRE(runner.step(false)==sim::Result::ok);
 const auto digest=runner.snapshot_sha256();
 fid::Bundle bundle;REQUIRE(fid::Bundle::load(cfg,manifest(),project,bundle)==fid::Status::ok);
 fid::Report a,b;
 REQUIRE(fid::evaluate(cfg,scratch/"run",bundle,a)==fid::Status::ok);
 REQUIRE(fid::evaluate(cfg,scratch/"run",bundle,b)==fid::Status::ok);
 REQUIRE(a.json==b.json && a.sha256==b.sha256);
 REQUIRE(a.training_count==12 && a.holdout_count==16);
 REQUIRE(a.training_p90_band==2);
 REQUIRE(a.json.find("\"training_p90_abs_error_band\":2")!=std::string::npos);
 REQUIRE(a.json.find("\"mae\":0.750000")!=std::string::npos);
 REQUIRE(a.json.find("\"mae\":1.500000")!=std::string::npos);
 REQUIRE(a.json.find("\"independent_holdout_verified\":false")!=std::string::npos);
 REQUIRE(a.json.find("\"training_p90_band_coverage\":0.750000")!=std::string::npos);
 REQUIRE(a.json.find("\"per_metric\":{")!=std::string::npos);
 REQUIRE(a.json.find("\"energy\":{")!=std::string::npos);
 REQUIRE(runner.snapshot_sha256()==digest); // evaluation NEVER changes colony state
 db::Registry registry;
 REQUIRE(registry.open(scratch/"registry")==db::Code::ok);
 auto object=registry.put(a.json);REQUIRE(object.code==db::Code::ok);
 auto committed=registry.publish("MODEL_EVALUATION","sim3-fidelity-baseline",object.hash,"SIMULATED",6,"synthetic-tick","sim32-evaluator");
 REQUIRE(committed.code==db::Code::ok && registry.audit_disk());
 REQUIRE(registry.publish("MODEL_EVALUATION","sim3-fidelity-baseline",object.hash,"SIMULATED",6,"synthetic-tick","sim32-evaluator").code==db::Code::ok);
 REQUIRE(registry.event_count()==1);
 const auto hole=scratch/"run/history/tick-000004.state";
 std::error_code ec;fs::rename(hole,hole.string()+".missing",ec);REQUIRE(!ec);
 REQUIRE(fid::evaluate(cfg,scratch/"run",bundle,b)==fid::Status::missing_checkpoint);
 fs::rename(hole.string()+".missing",hole,ec);REQUIRE(!ec);
 {
   std::ofstream tamper(hole,std::ios::binary|std::ios::app);tamper<<"unauthorized";
 }
 REQUIRE(fid::evaluate(cfg,scratch/"run",bundle,b)==fid::Status::bad_snapshot);
}
int main(int argc,char**argv){
 REQUIRE(argc==2);project=fs::canonical(argv[1]);
 sim::Configuration cfg;REQUIRE(sim::Configuration::load(cfg_path(),cfg)==sim::Result::ok);
 test_validation(cfg);
 const auto scratch=fs::temp_directory_path()/"anaiero-sim32-model-fidelity-test";
 std::error_code ec;fs::remove_all(scratch,ec);fs::create_directories(scratch);
 test_execution_and_integrity(cfg,scratch);
 fs::remove_all(scratch,ec);
 std::cout<<"SIM3.2 frozen synthetic holdout, tamper, law and checkpoint tests passed\n";
}

#include "christeye/living/lab_core.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
namespace sim=christeye::living::sim3;
namespace lab=christeye::living::lab;
namespace fs=std::filesystem;
#define CHECK(e) do{if(!(e)){std::cerr<<"FAILED " #e " line "<<__LINE__<<'\n';std::exit(1);}}while(false)
static void put(const fs::path& file,const std::string& data){
  std::ofstream o(file,std::ios::binary|std::ios::trunc);CHECK(bool(o));o<<data;o.close();CHECK(bool(o));
}
int main(int argc,char**argv){
  CHECK(argc==2);
  const fs::path root(argv[1]);
  const auto cfgfile=root/"examples/connected-gardens.cev";
  const auto temp=fs::temp_directory_path()/"anaiero-sim31-unit";
  std::error_code ec;fs::remove_all(temp,ec);fs::create_directories(temp);
  sim::Configuration cfg;CHECK(sim::Configuration::load(cfgfile,cfg)==sim::Result::ok);
  CHECK(cfg.laws.blood_step==1 && cfg.laws.learning_gain==1);
  std::string tip;
  {
    sim::Runner runner;CHECK(runner.open(cfgfile,temp/"culture")==sim::Result::ok);
    for(int i=0;i<3;++i)CHECK(runner.step(false)==sim::Result::ok);
    tip=runner.snapshot_sha256();
  }
  {
    sim::Runner runner;CHECK(runner.open(cfgfile,temp/"culture")==sim::Result::ok);
    CHECK(runner.state().tick==3&&runner.snapshot_sha256()==tip);
    CHECK(lab::frame(cfg,runner.state()).find("\"format\":\"CEVLABFRAME/1\"")!=std::string::npos);
  }
  {
    christeye::living::durable::Registry registry;
    CHECK(registry.open(temp/"store")==christeye::living::durable::Code::ok);
    const auto first=lab::index_history(cfg,temp/"culture",registry,3);
    CHECK(first.code==lab::Code::ok&&first.indexed==4);
    const auto repeat=lab::index_history(cfg,temp/"culture",registry,3);
    CHECK(repeat.code==lab::Code::ok&&repeat.duplicates==4);
    CHECK(registry.event_count()==4&&registry.audit_disk());
    auto missing=temp/"culture/history/tick-000002.state";
    fs::rename(missing,missing.string()+".bak");
    CHECK(lab::index_history(cfg,temp/"culture",registry,3).code==lab::Code::bad_snapshot);
    fs::rename(missing.string()+".bak",missing);
  }
  {
    // Reference inputs are copied inside the private test root.
    auto fixture=temp/"fixture";fs::create_directories(fixture/"sim3");fs::create_directories(fixture/"vlm");
    fs::copy_file(root/"examples/sim3/source-demo.txt",fixture/"sim3/source-demo.txt");
    fs::copy_file(root/"examples/vlm/process-garden.avlm",fixture/"vlm/process-garden.avlm");
    std::string source;CHECK(sim::bounded_read(cfgfile,sim::kMaxConfigBytes,source));
    auto at=source.find("end\n");CHECK(at!=std::string::npos);
    source.insert(at,"law_version 1\nlaw blood_step 3\nlaw learning_gain 4\n");
    put(fixture/"law.cev",source);
    sim::Configuration tuned;CHECK(sim::Configuration::load(fixture/"law.cev",tuned)==sim::Result::ok);
    CHECK(tuned.laws.blood_step==3&&tuned.hash!=cfg.hash);
    sim::Runner original,variant;
    CHECK(original.open(cfgfile,temp/"base")==sim::Result::ok);
    CHECK(variant.open(fixture/"law.cev",temp/"variant")==sim::Result::ok);
    for(int i=0;i<4;++i){CHECK(original.step(false)==sim::Result::ok);CHECK(variant.step(false)==sim::Result::ok);}
    CHECK(original.snapshot_sha256()!=variant.snapshot_sha256());
    source.insert(source.find("end\n"),"law blood_step 9\n");
    put(fixture/"law.cev",source);
    CHECK(sim::Configuration::load(fixture/"law.cev",tuned)==sim::Result::bad_config);
  }
  {
    const std::string profile="CEVMODEL/1\nname synthetic-test\ndomain SYNTHETIC_ONLY\nsource_sha256 "+cfg.material_sha+
         "\nreference alive 4 1\nreference energy 120 100\nend\n";
    put(temp/"test.cevmodel",profile);
    lab::Profile model;CHECK(lab::Profile::read(temp/"test.cevmodel",model));
    sim::Runner run;CHECK(run.open(cfgfile,temp/"culture")==sim::Result::ok);
    CHECK(lab::compare(model,cfg,run.state()).find("NOT_BIOLOGICAL_VALIDATION")!=std::string::npos);
  }
  fs::remove_all(temp,ec);
  std::cout<<"SIM3.1 checkpoint index, laws and synthetic comparison tests passed\n";
}

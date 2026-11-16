#include "christeye/living/colony_runner.hpp"
#include "christeye/living/mapped_culture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <string>
namespace sim=christeye::living::sim3;
namespace fs=std::filesystem;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL " #x " at "<<__LINE__<<'\n';std::exit(1);}}while(false)
static void write(const fs::path& p,std::string_view data){
  fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary|std::ios::trunc);
  CHECK(static_cast<bool>(out));out<<data;out.close();CHECK(static_cast<bool>(out));
}
static std::string read(const fs::path& p){std::string text;CHECK(sim::bounded_read(p,sim::kMaxSnapshotBytes,text));return text;}
static fs::path scenario(const fs::path& root){
  auto assets=root/"assets";fs::create_directories(assets);
  const std::string template_text="CEVLM/2\nname synthetic-source\nmodel 1\nimage\nspawn 1 crest_progenitor 10 12 0\nendimage\njournal\nendjournal\nseal source-template-placeholder\n";
  const std::string profile="CEVSOURCE/1\nsource synthetic\ncategory synthetic\nbiology NOT_MEASURED\nmodel SIM3_UNCALIBRATED\n";
  write(assets/"base.avlm",template_text);
  write(assets/"profile.txt",profile);
  const auto sha=sim::pl::sha256_hex(template_text),prof=sim::pl::sha256_hex(profile);
  const std::string config="CEVRUN/1\nname twin-garden\nsource synthetic assets/profile.txt "+prof+"\n"
      "colony crest assets/base.avlm "+sha+"\ncolony receiving assets/base.avlm "+sha+"\n"
      "seed crest 1 crest_progenitor 10 12 2 0\n"
      "seed crest 2 blood_carrier 3 5 2 0\n"
      "seed receiving 3 neuron 20 17 4 0\n"
      "link crest receiving food 3\nlink crest receiving data 4\n"
      "link receiving crest stimubus 2\n"
      "process crest support 0 5 1\n"
      "process crest divide 1 4 2\n"
      "process receiving teach 3 8 2\n"
      "process receiving foreign_stress 0 13 3\nend\n";
  write(root/"example.cev",config);return root/"example.cev";
}
static void run_tests(const fs::path& base){
  auto tmp=fs::temp_directory_path()/"anaiero-sim3-tests";
  std::error_code ec;fs::remove_all(tmp,ec);fs::create_directories(tmp);
  auto config=scenario(tmp);
  sim::Configuration parsed;CHECK(sim::Configuration::load(config,parsed)==sim::Result::ok);
  CHECK(parsed.colonies.size()==2 && parsed.seeds.size()==3 && parsed.links.size()==3);
  CHECK(sim::safe_relative("assets/base.avlm"));
  CHECK(!sim::safe_relative("../secrets.txt"));
  CHECK(!sim::safe_relative("https://example.com/data"));
  std::string eventual_hash;
  {
    sim::Runner runner;
    CHECK(runner.open(config,tmp/"active")==sim::Result::ok);
    CHECK(runner.state().tick==0 && runner.audit_disk());
    CHECK(runner.step()==sim::Result::ok);
    CHECK(runner.step()==sim::Result::ok);
    CHECK(read(tmp/"active/history/tick-000001.state").starts_with("CEVRUNSTATE/1"));
    CHECK(read(tmp/"active/history/tick-000002.state")==read(tmp/"active/culture.state"));
    CHECK(runner.state().tick==2 && runner.state().cells.size()==4);
    const auto& child=runner.state().cells.back();
    CHECK(child.id==4 && child.parent==1 && child.birth==2 && child.alive);
    CHECK(read(tmp/"active/frames/tick-000001.svg").starts_with("<svg"));
    CHECK(read(tmp/"active/frames/tick-000002.obj").find("g circuit_crest_receiving_data")!=std::string::npos);
    sim::MappedCulture snapshot;
    CHECK(snapshot.open(tmp/"active/history/tick-000002.state"));
    const auto old=std::string(snapshot.bytes().data(),snapshot.bytes().size());
    CHECK(old==read(tmp/"active/culture.state"));
    CHECK(runner.step()==sim::Result::ok);
    CHECK(old!=read(tmp/"active/culture.state"));
    CHECK(std::string(snapshot.bytes().data(),snapshot.bytes().size())==old);
    CHECK(runner.state().tick==3);
    // An active writer holds an OS exclusive lock for this directory.
    sim::Runner contender;
    CHECK(contender.open(config,tmp/"active")==sim::Result::locked);
    eventual_hash=runner.snapshot_sha256();
    CHECK(read(tmp/"active/history/tick-000003.state")==read(tmp/"active/culture.state"));
    CHECK(runner.audit_disk());
  }
  {
    sim::Runner runner;
    CHECK(runner.open(config,tmp/"active")==sim::Result::ok);
    CHECK(runner.snapshot_sha256()==eventual_hash && runner.state().tick==3);
    for(int i=0;i<2;++i)CHECK(runner.step()==sim::Result::ok);
    CHECK(runner.state().tick==5 && runner.state().cells.size()==4);
    CHECK(read(tmp/"active/history/tick-000005.state")==read(tmp/"active/culture.state"));
    CHECK(runner.state().colonies[1].data>0);
    CHECK(runner.state().cells[2].learning>8); // teaching + synthetic data route
    eventual_hash=runner.snapshot_sha256();
    CHECK(runner.audit_disk());
  }
  {
    sim::Runner fresh;
    CHECK(fresh.open(config,tmp/"deterministic")==sim::Result::ok);
    for(int i=0;i<5;++i)CHECK(fresh.step()==sim::Result::ok);
    CHECK(fresh.snapshot_sha256()==eventual_hash);
    const auto image=sim::svg(fresh.state(),fresh.configuration());
    const auto obj=sim::obj(fresh.state(),fresh.configuration());
    CHECK(obj.find("g lineage_crest")!=std::string::npos);
    CHECK(obj.find("g circuit_crest_receiving_food")!=std::string::npos);
    CHECK(image.find("Anaiero SIM3")!=std::string::npos);
    const auto golden=base/"tests/golden/sim3-tick-000005.svg";
    CHECK(fs::exists(golden) && image==read(golden));
    const auto gold_obj=base/"tests/golden/sim3-tick-000005.obj";
    CHECK(fs::exists(gold_obj) && obj==read(gold_obj));
  }
  // Tombstones: the simulation retains parent/child lineage when cells die.
  {
    auto depleted=read(config);
    const std::string replenishment="process crest support 0 5 1\n";
    const auto pos=depleted.find(replenishment);
    CHECK(pos!=std::string::npos);
    depleted.erase(pos,replenishment.size());
    write(tmp/"depleted.cev",depleted);
    sim::Runner dying;
    CHECK(dying.open(tmp/"depleted.cev",tmp/"long-run")==sim::Result::ok);
    for(int i=0;i<140;++i)CHECK(dying.step(false)==sim::Result::ok);
    CHECK(dying.state().cells.size()==4);
    bool dead=false;
    for(const auto& cell:dying.state().cells)if(!cell.alive){dead=true;CHECK(cell.death>0);}
    CHECK(dead); // eventually resource exhaustion or sustained stress
    CHECK(dying.audit_disk());
  }
  // A modified source cannot initialize a new run; corrupted committed state
  // does not silently reset a mature virtual culture.
  {
    const auto text=read(tmp/"assets/profile.txt");
    write(tmp/"assets/profile.txt",text+"CHANGED\n");
    sim::Runner rejected;
    CHECK(rejected.open(config,tmp/"new-run")==sim::Result::bad_hash);
    write(tmp/"assets/profile.txt",text);
    write(tmp/"active/culture.state","corrupted");
    CHECK(rejected.open(config,tmp/"active")==sim::Result::corruption);
  }
  fs::remove_all(tmp,ec);
}
int main(int argc,char**argv){CHECK(argc==2);run_tests(argv[1]);
  std::cout<<"SIM3 two colonies, restart, mmap, lineage, stress, visualization tests passed\n";
}

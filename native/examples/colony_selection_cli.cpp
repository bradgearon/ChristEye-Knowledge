// SIM3.3 host-only model selection; no access to holdout on prepare/select.
#include "christeye/living/model_selection.hpp"
#include "christeye/living/durable_registry.hpp"
#include <filesystem>
#include <iostream>
#include <string_view>
namespace fs=std::filesystem;
namespace sl=christeye::living::selection;
namespace sim=christeye::living::sim3;
namespace os=christeye::living::durable::os;
namespace db=christeye::living::durable;
static int error(const char* stage,sl::Error code){
 std::cerr<<stage<<" error="<<static_cast<unsigned>(code)<<'\n';return 1;
}
static int usage(){
 std::cerr<<"usage: ce_colony_select prepare <manifest.cevselect> <artifact-root> <runs-root>\n"
          <<"       ce_colony_select select <manifest.cevselect> <artifact-root> <runs-root> <selection.cevwinner>\n"
          <<"       ce_colony_select final <manifest.cevselect> <artifact-root> <runs-root> <selection.cevwinner> <frozen-test.cevsplits> <report.json> [windstore]\n";
 return 2;
}
int main(int argc,char**argv){
 if(argc<5)return usage();
 const std::string_view command(argv[1]);
 const bool prepare=command=="prepare",select=command=="select",final=command=="final";
 if(!prepare&&!select&&!final)return usage();
 if((prepare&&argc!=5)||(select&&argc!=6)||(final&&argc!=8&&final&&argc!=9))return usage();
 std::string content;
 if(!sim::bounded_read(argv[2],sl::fid::kMaxManifestBytes,content))return error("manifest",sl::Error::io_error);
 sl::Manifest manifest;
 if(auto r=sl::Manifest::parse(content,manifest);r!=sl::Error::ok)return error("manifest",r);
 const fs::path root(argv[3]),runs(argv[4]);
 if(prepare){
   // Prepare does not open the test dataset. Its SHA is a frozen commitment.
   for(const auto& c:manifest.candidates){
     sim::Configuration config;
     if(auto r=sl::candidate_config(c,root,manifest.source_sha256,config);r!=sl::Error::ok)return error("candidate",r);
     fs::path config_file;
     if(!sim::bound_path(root,c.config_path,config_file))return error("config-path",sl::Error::bad_configuration);
     sim::Runner runner;
     if(runner.open(config_file,runs/c.name)!=sim::Result::ok)return error("runner",sl::Error::bad_configuration);
     if(runner.state().tick>manifest.horizon)return error("horizon",sl::Error::mismatch);
     while(runner.state().tick<manifest.horizon)
       if(runner.step(false)!=sim::Result::ok)return error("step",sl::Error::bad_snapshot);
     if(!runner.audit_disk())return error("audit",sl::Error::bad_snapshot);
     std::cout<<c.name<<" prepared tick="<<runner.state().tick<<"\n";
   }
   return 0;
 }
 if(select){
   if(fs::exists(argv[5]))return error("overwrite",sl::Error::io_error);
   sl::Selection result;
   if(auto r=sl::select(manifest,root,runs,result);r!=sl::Error::ok)return error("select",r);
   if(!os::atomic_write(fs::absolute(argv[5]),result.report))return error("write",sl::Error::io_error);
   std::cout<<"{\"winner\":\""<<result.winner<<"\",\"selection_sha256\":\""
            <<result.sha256<<"\",\"candidate_count\":"<<result.ranked.size()<<"}\n";
   return 0;
 }
 if(fs::exists(argv[7]))return error("overwrite",sl::Error::io_error);
 std::string winner_bytes;
 if(!sim::bounded_read(argv[5],sl::fid::kMaxManifestBytes,winner_bytes))return error("winner",sl::Error::io_error);
 sl::FinalReport report;
 if(auto r=sl::final_test(manifest,root,runs,winner_bytes,argv[6],report);r!=sl::Error::ok)return error("final",r);
 if(!os::atomic_write(fs::absolute(argv[7]),report.json))return error("write",sl::Error::io_error);
 if(argc==9){
   db::Registry registry;
   if(registry.open(argv[8])!=db::Code::ok)return error("store",sl::Error::io_error);
   const auto object=registry.put(report.json);
   if(object.code!=db::Code::ok)return error("store-object",sl::Error::io_error);
   sl::Selection winner;
   if(sl::read_winner(winner_bytes,manifest,winner)!=sl::Error::ok)return error("winner",sl::Error::mismatch);
   const auto event=registry.publish("MODEL_EVALUATION",manifest.name,object.hash,"SIMULATED",manifest.horizon,"synthetic-tick","sim3-selection");
   if(event.code!=db::Code::ok)return error("store-event",sl::Error::io_error);
 }
 std::cout<<"{\"report_sha256\":\""<<report.sha256<<"\",\"test_rows\":"<<report.count
          <<",\"test_mae\":"<<report.mae<<"}\n";
 return 0;
}

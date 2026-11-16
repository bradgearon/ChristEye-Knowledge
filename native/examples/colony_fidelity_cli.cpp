// CE-SW-14: immutable reference evaluation against committed SIM3 checkpoints.
// All numerical targets are uncalibrated synthetic fixtures; no biological claims.
#include "christeye/living/model_fidelity.hpp"
#include "christeye/living/durable_registry.hpp"
#include <iostream>
#include <string_view>
namespace fid=christeye::living::fidelity;
namespace sim=christeye::living::sim3;
namespace fs=std::filesystem;
namespace db=christeye::living::durable;
static int fail(const char* text,int code){std::cerr<<text<<": "<<code<<'\n';return 1;}
int main(int argc,char** argv){
 if(argc!=7 && argc!=8){
   std::cerr<<"usage: ce_colony_fidelity evaluate <config.cev> <history-root> "
               "<evaluation.ceveval> <trusted-artifact-root> <report.json> [registry-root]\n";
   return 2;
 }
 if(std::string_view(argv[1])!="evaluate")return 2;
 sim::Configuration cfg;
 if(auto s=sim::Configuration::load(argv[2],cfg);s!=sim::Result::ok)
   return fail("configuration",int(s));
 fid::Bundle b;
 if(auto s=fid::Bundle::load(cfg,argv[4],argv[5],b);s!=fid::Status::ok)
   return fail("fidelity profile",int(s));
 fid::Report report;
 if(auto s=fid::evaluate(cfg,argv[3],b,report);s!=fid::Status::ok)
   return fail("evaluation",int(s));
 std::error_code ec;
 if(fs::exists(argv[6],ec)||ec)return fail("output exists",1);
 if(!christeye::living::durable::os::atomic_write(argv[6],report.json))
   return fail("report commit",1);
 if(argc==8){
   db::Registry registry;
   if(auto s=registry.open(argv[7]);s!=db::Code::ok)return fail("registry",int(s));
   const auto obj=registry.put(report.json);
   if(obj.code!=db::Code::ok)return fail("object",int(obj.code));
   const auto event=registry.publish("MODEL_EVALUATION",b.manifest.name,obj.hash,
                                      "SIMULATED",0,"synthetic-tick","sim32-evaluator");
   if(event.code!=db::Code::ok)return fail("publish",int(event.code));
 }
 std::cout<<"{\"report_sha256\":\""<<report.sha256<<"\",\"train\":"
          <<report.training_count<<",\"holdout\":"<<report.holdout_count<<"}\n";
 return 0;
}

// Host-side SIM3 digital colony runner; no physical instrument access.
#include "christeye/living/colony_runner.hpp"
#include "christeye/living/mapped_culture.hpp"
#include <iostream>
#include <string_view>
namespace sim=christeye::living::sim3;
static int fail(sim::Result r){std::cerr<<"SIM3 error="<<static_cast<unsigned>(r)<<'\n';return 1;}
int main(int argc,char** argv){
  if(argc<4){std::cerr<<"Usage: ce_colony_runner <inspect|step|svg|obj|mapped> <run.cev> <store-dir> [steps]\n";return 2;}
  const std::string_view action(argv[1]);
  sim::Runner runner;
  const auto opened=runner.open(argv[2],argv[3]);
  if(opened!=sim::Result::ok)return fail(opened);
  if(action=="step"){
    std::uint32_t n=1;
    if(argc==5 && (!sim::parse_u32(argv[4],n)||n==0||n>256))return 2;
    for(std::uint32_t i=0;i<n;++i){auto r=runner.step();if(r!=sim::Result::ok)return fail(r);}
  }else if(action=="svg")std::cout<<sim::svg(runner.state(),runner.configuration());
  else if(action=="obj")std::cout<<sim::obj(runner.state(),runner.configuration());
  else if(action=="mapped"){
    sim::MappedCulture mapped;
    // Map the immutable revision; mapping a replaceable HEAD blocks renames on Windows.
    if(!mapped.open(runner.committed_history_path()))return 1;
    std::cout.write(mapped.bytes().data(),static_cast<std::streamsize>(mapped.bytes().size()));
  }else if(action!="inspect")return 2;
  if(action=="inspect"||action=="step"){
    std::cout<<"{\"origin\":\"SIMULATED_UNCALIBRATED\",\"tick\":"<<runner.state().tick
      <<",\"colonies\":"<<runner.state().colonies.size()<<",\"cells\":"<<runner.state().cells.size()
      <<",\"snapshot_sha256\":\""<<runner.snapshot_sha256()<<"\",\"disk_audit\":"
      <<(runner.audit_disk()?"true":"false")<<"}\n";
  }
  return 0;
}

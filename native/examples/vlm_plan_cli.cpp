// Host-only digital learning-plan and Crystal evidence mapping CLI.
#include "christeye/living/learning_plan.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
namespace pl = christeye::living::plans;
namespace p = christeye::living::process;
namespace v = christeye::living::vlm;
static int fail(std::string_view action, unsigned code){
  std::cerr << action << " failed: " << code << '\n';return 1;
}
static void on_event(void* ptr,const pl::Progress& e) noexcept {
  auto& out=*static_cast<std::ostream*>(ptr);
  pl::write_progress_json(out,e); // host diagnostic sink, not a realtime audio callback
}
int main(int argc,char**argv){
  if(argc<3){std::cerr<<"Commands: ce_vlm_plan basehash <base.avlm> | "
    "inspect|stream <plan.cevplan> <base.avlm> <artifact-root> | "
    "run <plan.cevplan> <base.avlm> <artifact-root> <output.avlm> [receipts.ndjson]\n";return 2;}
  const std::string_view command{argv[1]};
  if(command=="basehash"){
    if(argc!=3)return 2;
    p::ProcessMachine base;
    const auto r=p::read_file(argv[2],base);
    if(r.error!=p::Error::ok)return fail("base",static_cast<unsigned>(r.error));
    std::cout << pl::machine_sha256(base) << '\n';return 0;
  }
  if(!(command=="inspect" || command=="run" || command=="stream"))return 2;
  if((command=="run" && (argc!=6 && argc!=7)) ||
     (command!="run" && argc!=5))return 2;
  pl::Plan plan;
  if(auto r=pl::read_plan(argv[2],plan);r.error!=pl::Error::ok)
    return fail("plan",static_cast<unsigned>(r.error));
  p::ProcessMachine base;
  if(auto r=p::read_file(argv[3],base);r.error!=p::Error::ok)
    return fail("base",static_cast<unsigned>(r.error));
  pl::Session session;
  if(auto r=session.initialize(plan,base,argv[4]);r!=pl::Error::ok)
    return fail("verify mappings",static_cast<unsigned>(r));
  if(command=="inspect"){
    std::cout<<"{\"class\":\"SIMULATED_UNCALIBRATED\",\"plan\":\""
             <<plan.name()<<"\",\"plan_sha256\":\""<<plan.hash()
             <<"\",\"base_sha256\":\""<<plan.base_sha()
             <<"\",\"artifacts\":"<<plan.artifacts().size()
             <<",\"stages\":"<<plan.stages().size()<<"}\n";
    return 0;
  }
  std::ostringstream buffered;
  std::ostream* sink=command=="stream"?&std::cout:&buffered;
  session.subscribe(on_event,sink);
  std::size_t ticks=0;
  while(!session.finished()){
    if(++ticks>pl::kMaxStages*512u)return fail("tick limit",1);
    if(auto r=session.step();r!=pl::Error::ok)
      return fail("stage execution",static_cast<unsigned>(r));
  }
  if(command=="stream")return 0;
  // Do not publish the output machine until the entire synthetic plan succeeds.
  if(auto r=p::write_file(argv[5],session.machine());r!=p::Error::ok)
    return fail("machine write",static_cast<unsigned>(r));
  if(argc==7){
    const auto tmp=std::string(argv[6])+".tmp";
    std::ofstream f(tmp,std::ios::binary|std::ios::trunc);
    if(!f)return fail("receipt write",1);
    f<<buffered.str();f.flush();
    if(!f)return fail("receipt flush",1);
    f.close();
    std::error_code ec;
    std::filesystem::rename(tmp,argv[6],ec);
    if(ec)return fail("receipt rename",1);
  }
  // Sidecar is the reverse index from the new VLM image to its plan,
  // associated Crystal/learning/RNA/support records and receipt stream.
  const auto sidecar=std::string(argv[5])+".cevmap";
  const auto tmp=sidecar+".tmp";
  {
    std::ofstream map(tmp,std::ios::binary|std::ios::trunc);
    if(!map)return fail("mapping write",1);
    map<<pl::mapping_index(plan,session.machine(),buffered.str());
    map.flush();
    if(!map)return fail("mapping flush",1);
  }
  {
    std::error_code ec;
    std::filesystem::rename(tmp,sidecar,ec);
    if(ec)return fail("mapping rename",1);
  }
  std::cout << "{\"class\":\"SIMULATED_UNCALIBRATED\",\"completed_stages\":"
            << plan.stages().size() << ",\"ticks\":" << session.machine().tick()
            << ",\"plan_sha256\":\"" << plan.hash() << "\",\"machine_sha256\":\""
            << pl::machine_sha256(session.machine()) << "\"}\n";
  return 0;
}

// Host-only SW2.1 command line. Evidence events remain simulated/unverified.
#include "christeye/living/durable_registry.hpp"
#ifdef CE_DURABLE_WITH_LEARNING_PLAN
#include "christeye/living/learning_plan.hpp"
#endif
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
namespace d=christeye::living::durable;
namespace fs=std::filesystem;
static int fail(d::Code code){std::cerr<<"SW2.1 failure code="<<static_cast<unsigned>(code)<<'\n';return 1;}
static int usage(){
 std::cerr<<"Commands:\n"
          <<"  ce_wind_durable verify <store>\n"
          <<"  ce_wind_durable import <store> <artifact>\n"
          <<"  ce_wind_durable publish <store> <kind> <entity> <object-sha> <origin> <tick> <clock>\n"
          <<"  ce_wind_durable events <store>\n"
          <<"  ce_wind_durable cursor <store> <consumer>\n"
          <<"  ce_wind_durable ack <store> <consumer> <sequence> <event-sha>\n"
          <<"  ce_wind_durable catchup <store> <consumer> [entity [kind]]\n"
          <<"  ce_wind_durable register-plan <store> <plan.cevplan> <artifact-root>\n";
 return 2;
}
static void print_event(void*,const d::Event& e) noexcept {std::cout<<d::serialize_event(e);}
int main(int argc,char** argv){
 if(argc<3)return usage();
 const std::string_view op(argv[1]);
 d::Registry store;
 const auto opened=store.open(argv[2]);
 if(opened!=d::Code::ok)return fail(opened);
 if(op=="verify"&&argc==3){
  if(!store.audit_disk())return fail(d::Code::corrupt_journal);
  std::cout<<"SW2.1 verified committed events="<<store.event_count()<<'\n';return 0;
 }
 if(op=="events"&&argc==3){
  for(const auto& e:store.events())std::cout<<d::serialize_event(e);
  return 0;
 }
 if(op=="import"&&argc==4){
  std::string data;
  if(!d::read_bounded(argv[3],d::kMaxObjectBytes,data))return fail(d::Code::io_error);
  auto result=store.put(data);
  if(result.code!=d::Code::ok)return fail(result.code);
  std::cout<<result.hash<<'\n';return 0;
 }
 if(op=="publish"&&argc==9){
  std::uint64_t tick=0;
  if(!d::parse_u64(argv[7],tick))return fail(d::Code::invalid_argument);
  auto r=store.publish(argv[3],argv[4],argv[5],argv[6],tick,argv[8]);
  if(r.code!=d::Code::ok)return fail(r.code);
  std::cout<<"committed_event_sha256 "<<r.hash<<'\n';return 0;
 }
 if(op=="cursor"&&argc==4){
  auto c=store.read_cursor(argv[3]);
  if(c.code!=d::Code::ok)return fail(c.code);
  std::cout<<"sequence "<<c.sequence<<" event_sha256 "<<c.hash<<'\n';return 0;
 }
 if(op=="ack"&&argc==6){
  std::uint64_t seq=0;
  if(!d::parse_u64(argv[4],seq))return fail(d::Code::invalid_cursor);
  const auto result=store.acknowledge(argv[3],seq,argv[5]);
  if(result!=d::Code::ok)return fail(result);
  std::cout<<"acked "<<seq<<'\n';return 0;
 }
 if(op=="catchup"&&(argc>=4&&argc<=6)){
  std::string entity=argc>=5?argv[4]:"";
  std::string kind=argc>=6?argv[5]:"";
  const auto result=store.resume(print_event,nullptr,argv[3],entity,kind);
  return result==d::Code::ok?0:fail(result);
 }
#ifdef CE_DURABLE_WITH_LEARNING_PLAN
 if(op=="register-plan"&&argc==5){
  christeye::living::plans::Plan plan;
  if(auto result=christeye::living::plans::read_plan(argv[3],plan);
     result.error!=christeye::living::plans::Error::ok)
    return fail(d::Code::invalid_argument);
  auto result=store.register_plan(plan,fs::path(argv[4]));
  if(result.code!=d::Code::ok)return fail(result.code);
  std::cout<<"plan_object_sha256 "<<result.hash<<'\n';return 0;
 }
#endif
 return usage();
}

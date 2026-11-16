// CE-SW-10 digital-only registry CLI: local immutable objects and observations.
#include "christeye/living/wind_registry.hpp"
#include <cstdlib>
#include <iostream>
#include <string_view>
namespace reg=christeye::living::registry;
namespace pl=christeye::living::plans;
static int fail(unsigned n){std::cerr<<"registry error="<<n<<'\n';return 1;}
int main(int argc,char**argv){
  if(argc<3){std::cerr<<"Commands: verify <store> | import <store> <file> | "
  "register-plan <store> <plan> <artifact-root> | "
  "observe <store> <kind> <entity> <digest> <origin> <tick> <clock> | "
  "successor <store> <parent-plan-object-digest> <new-learning-digest> "
  "<evidence-digest> <candidate-id> | events <store>\n";return 2;}
  reg::Registry store;
  if(auto s=store.open(argv[2]);s!=reg::Code::ok)return fail(static_cast<unsigned>(s));
  const std::string_view op(argv[1]);
  if(op=="verify" && argc==3){
    if(!store.verify())return fail(2);
    std::cout<<"verified events="<<store.events().size()<<'\n';return 0;
  }
  if(op=="events" && argc==3){
    for(const auto& event:store.events())std::cout<<reg::serialize_event(event);
    return 0;
  }
  if(op=="import" && argc==4){
    std::string content;
    if(!pl::read_bounded(argv[3],reg::kMaxRegistryObjectBytes,content))return fail(3);
    auto r=store.put(content);
    if(r.status!=reg::Code::ok)return fail(static_cast<unsigned>(r.status));
    std::cout<<r.hash<<'\n';return 0;
  }
  if(op=="register-plan"&&argc==5){
    pl::Plan plan;
    if(pl::read_plan(argv[3],plan).error!=pl::Error::ok)return fail(4);
    auto r=store.register_plan(plan,argv[4]);
    if(r.status!=reg::Code::ok)return fail(static_cast<unsigned>(r.status));
    std::cout<<"plan_object_sha256 "<<r.hash<<'\n';return 0;
  }
  if(op=="observe"&&argc==9){
    std::uint64_t tick=0;
    if(!reg::parse_u64(argv[7],tick))return fail(5);
    auto r=store.publish(argv[3],argv[4],argv[5],argv[6],tick,argv[8]);
    if(r.status!=reg::Code::ok)return fail(static_cast<unsigned>(r.status));
    std::cout<<"event_sha256 "<<r.hash<<'\n';return 0;
  }
  if(op=="successor"&&argc==7){
    std::string old;
    if(!store.get(argv[3],old)||!old.starts_with("CEVPLAN/1\n"))return fail(6);
    if(!store.get(argv[4],old)||!old.starts_with("CELEARN/1\n"))return fail(7);
    if(!store.get(argv[5],old))return fail(8);
    const auto candidate=reg::successor_manifest(argv[3],argv[4],argv[5],argv[6]);
    if(candidate.empty())return fail(9);
    auto r=store.put(candidate);
    if(r.status!=reg::Code::ok)return fail(static_cast<unsigned>(r.status));
    r=store.publish("SUCCESSOR_PROPOSED",argv[6],r.hash,"IMPORTED_UNVERIFIED",0,"proposal-clock");
    if(r.status!=reg::Code::ok)return fail(static_cast<unsigned>(r.status));
    std::cout<<"successor_event_sha256 "<<r.hash<<'\n';return 0;
  }
  return 2;
}

#pragma once
// Strict process protocol. Any failed exchange poisons this solver epoch.
#include "anaiero/contracts/supervised_solver.hpp"
#include "anaiero/contracts/scientific_observable.hpp"
#include "christeye/living/plan_hash.hpp"
#include <array>
#include <charconv>
#include <sstream>
namespace anaiero::science {
class SupervisedSolverExchange final:public ISupervisedSolver {
  process::IProcessExchange& process_;
  SolverIdentity identity_{};SolverSample last_{};
  std::string cached_checkpoint_;
  std::uint32_t timeout_;
  bool initialized_=false,failed_=false;
  static bool digest(std::string_view text)noexcept{
    if(text.size()!=64)return false;
    for(char c:text)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
  }
  bool fail()noexcept{failed_=true;process_.stop();return false;}
  bool exchange(const std::string& request,std::string& reply)noexcept{
    return !failed_&&process_.exchange(request,reply,timeout_)==process::Code::ok?true:fail();
  }
  static bool sample(std::istream& input,SolverSample& s){
    return static_cast<bool>(input>>s.sequence>>s.elapsed_ns>>s.value>>s.uncertainty)&&s.uncertainty>=0;
  }
  bool valid_sample(const SolverSample& value)const noexcept{
    return value.sequence<=identity_.max_steps&&value.sequence<=UINT64_MAX/identity_.step_ns&&
      value.elapsed_ns==value.sequence*identity_.step_ns&&value.uncertainty>=0&&
      value.value>=INT32_MIN&&value.value<=INT32_MAX;
  }
public:
  SupervisedSolverExchange(process::IProcessExchange& process,std::uint32_t timeout_ms):process_(process),timeout_(timeout_ms){}
  bool initialize(const SolverIdentity& expected)override{
    if(initialized_||failed_||!digest(expected.model_sha256)||!digest(expected.behavior_sha256)||
       expected.provider.empty()||expected.provider.size()>64||!expected.epoch||!expected.step_ns||
       expected.step_ns>1000000000ULL||!expected.max_steps||expected.max_steps>1000000||
       !observable_for_unit(expected.unit)||expected.checkpoint_schema>1||!timeout_||timeout_>60000)return fail();
    for(char c:expected.provider)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='.'))return fail();
    identity_=expected;
    std::ostringstream request;request<<"HELLO 1 "<<expected.model_sha256<<' '<<expected.behavior_sha256<<' '
      <<expected.epoch<<' '<<expected.step_ns;
    std::string reply;if(!exchange(request.str(),reply))return false;
    std::istringstream row(reply);std::string tag,model,behavior,provider,extra;
    std::uint32_t version=0,unit=0,checkpoint=0;std::uint64_t epoch=0,step=0,limit=0;
    if(!(row>>tag>>version>>model>>behavior>>epoch>>step>>unit>>checkpoint>>limit>>provider)||
       tag!="READY"||version!=1||row>>extra||model!=expected.model_sha256||behavior!=expected.behavior_sha256||
       provider!=expected.provider||epoch!=expected.epoch||step!=expected.step_ns||
       unit!=static_cast<unsigned>(expected.unit)||checkpoint!=expected.checkpoint_schema||limit<expected.max_steps)return fail();
    initialized_=true;
    std::string latest;if(!exchange("LATEST",latest))return false;
    std::istringstream data(latest);std::string state;
    if(!(data>>state)||state!="SAMPLE"||!sample(data,last_)||data>>extra||!valid_sample(last_)||last_.sequence!=0)return fail();
    return true;
  }
  bool step(std::uint64_t expected_sequence,std::int32_t input,SolverSample& out)noexcept override{
    if(!initialized_||failed_||expected_sequence!=last_.sequence||expected_sequence>=identity_.max_steps||input<0||input>1000)return fail();
    std::ostringstream request;request<<"STEP "<<identity_.epoch<<' '<<expected_sequence+1<<' '<<input;
    std::string reply;if(!exchange(request.str(),reply))return false;
    std::istringstream row(reply);std::string tag,extra;SolverSample next{};
    if(!(row>>tag)||tag!="SAMPLE"||!sample(row,next)||row>>extra||
       next.sequence!=expected_sequence+1||!valid_sample(next))return fail();
    last_=next;cached_checkpoint_.clear();out=next;return true;
  }
  bool checkpoint(std::string& output)noexcept override{
    if(!initialized_||failed_||identity_.checkpoint_schema!=1)return false;
    if(!cached_checkpoint_.empty()){output=cached_checkpoint_;return true;}
    std::string reply;if(!exchange("CHECKPOINT",reply))return false;
    if(!reply.starts_with("STATE ")||reply.size()<=6||reply.size()>4096)return fail();
    // Bind opaque provider state to the latest sampled sequence and all identities.
    std::ostringstream body;body<<"CEVSOLVERSTATE/1\n"<<identity_.model_sha256<<' '<<identity_.behavior_sha256<<' '
      <<identity_.provider<<' '<<identity_.epoch<<' '<<identity_.step_ns<<' '<<static_cast<unsigned>(identity_.unit)<<' '
      <<identity_.checkpoint_schema<<'\n'<<last_.sequence<<' '<<last_.elapsed_ns<<' '<<last_.value<<' '<<last_.uncertainty<<'\n'<<reply.substr(6)<<'\n';
    const auto text=body.str();cached_checkpoint_=text+"seal "+christeye::living::plans::sha256_hex(text)+"\n";output=cached_checkpoint_;return true;
  }
  bool restore(std::string_view bytes,SolverSample& output)noexcept override{
    if(!initialized_||failed_||identity_.checkpoint_schema!=1||bytes.size()>8192||last_.sequence!=0)return false;
    const auto seal=bytes.rfind("seal ");
    if(seal==bytes.npos||bytes.size()!=seal+70||bytes.back()!='\n'||
       christeye::living::plans::sha256_hex(bytes.substr(0,seal))!=bytes.substr(seal+5,64))return fail();
    std::istringstream stream(std::string(bytes.substr(0,seal)));std::string header,model,behavior,provider,opaque,extra;
    std::uint64_t epoch=0,step=0;unsigned unit=0,schema=0;SolverSample restored{};
    if(!std::getline(stream,header)||header!="CEVSOLVERSTATE/1"||
       !(stream>>model>>behavior>>provider>>epoch>>step>>unit>>schema)||
       model!=identity_.model_sha256||behavior!=identity_.behavior_sha256||provider!=identity_.provider||
       epoch!=identity_.epoch||step!=identity_.step_ns||unit!=static_cast<unsigned>(identity_.unit)||schema!=1||
       !sample(stream,restored)||!valid_sample(restored))return fail();
    std::getline(stream,extra);if(!extra.empty()||!std::getline(stream,opaque)||opaque.empty()||opaque.size()>4096||std::getline(stream,extra))return fail();
    std::string reply;if(!exchange("RESTORE "+opaque,reply))return false;
    std::istringstream row(reply);std::string tag;SolverSample actual{};
    if(!(row>>tag)||tag!="SAMPLE"||!sample(row,actual)||row>>extra||actual.sequence!=restored.sequence||
       actual.elapsed_ns!=restored.elapsed_ns||actual.value!=restored.value||actual.uncertainty!=restored.uncertainty)return fail();
    last_=actual;cached_checkpoint_.clear();output=actual;return true;
  }
  bool latest(SolverSample& out)const noexcept override{if(!initialized_||failed_)return false;out=last_;return true;}
  bool failed()const noexcept override{return failed_;}
  void cancel()noexcept override{process_.cancel();}
};
} // namespace anaiero::science

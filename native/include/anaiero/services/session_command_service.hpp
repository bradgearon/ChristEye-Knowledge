#pragma once
#include "anaiero/contracts/host_runtime.hpp"
#include "anaiero/contracts/session_control.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include <charconv>
#include <cstring>
#include <sstream>
namespace anaiero::session {
class SessionCommandService final:public host::ICommandService {
  IControl& control_;
  contracts::IServiceExtension* extension_=nullptr;
  std::uint64_t requests_=0;
  static host::Reply reply(std::string_view text,bool stop=false)noexcept{
    host::Reply r{};
    if(text.empty()||text.size()>r.bytes.size())text="ERR response exceeds bounded frame\n";
    std::memcpy(r.bytes.data(),text.data(),text.size());r.length=text.size();r.stop=stop;return r;
  }
  static bool number(std::string_view value,std::uint32_t& out)noexcept{
    const auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),out);
    return ec==std::errc{}&&end==value.data()+value.size();
  }
public:
  explicit SessionCommandService(IControl& control)noexcept:control_(control){}
  void set_extension(contracts::IServiceExtension* extension)noexcept{extension_=extension;}
  host::Reply handle(std::string_view request)noexcept override{
    auto result=dispatch(request);++requests_;
    if(extension_)extension_->on_service_event({contracts::ServiceId::runtime_dispatch,
      contracts::ServicePhase::reviewed,requests_,0,result.stop?1:0});
    return result;
  }
private:
  host::Reply dispatch(std::string_view request)noexcept{
    if(request=="PING")return reply("PONG\n");
    if(request=="STATE"||request=="STATUS"||request=="ENGINES"||request=="SNAPSHOT")return reply(control_.snapshot());
    if(request=="RUN")return reply(control_.set_running(true)?"OK RUN\n":"ERR stopping\n");
    if(request=="PAUSE")return reply(control_.set_running(false)?"OK PAUSE\n":"ERR stopping\n");
    if(request=="CANCEL"){control_.cancel();return reply("OK CANCEL\n");}
    if(request=="STOP"){control_.request_stop();return reply("BYE\n",true);}
    std::istringstream input{std::string(request)};std::string word,extra;
    input>>word;
    if(word=="WATCH"){
      std::string text;std::uint64_t revision=0;
      if(!(input>>text)||input>>extra)return reply("ERR WATCH <revision>\n");
      const auto [end,ec]=std::from_chars(text.data(),text.data()+text.size(),revision);
      if(ec!=std::errc{}||end!=text.data()+text.size())return reply("ERR invalid revision\n");
      return reply(control_.snapshot(revision,500));
    }
    Command command{};
    if(word=="STEP"){
      if(input>>extra){if(!number(extra,command.count)||!command.count||command.count>1024)return reply("ERR STEP 1..1024\n");}
    }else if(word=="ANALYZE"){
      std::string id,from,to,horizon;command.operation=Operation::analyze;
      if(!(input>>id>>from>>to>>horizon)||!number(id,command.engine)||!number(from,command.from)||
         !number(to,command.to)||!number(horizon,command.count)||!command.count||command.count>128)
        return reply("ERR ANALYZE <engine> <from> <to> <horizon 1..128>\n");
    }else if(word=="APPLY")command.operation=Operation::apply;
    else if(word=="REVERT"){
      command.operation=Operation::revert;
      if(!(input>>extra)||!number(extra,command.engine))return reply("ERR REVERT <engine>\n");
    }else if(word=="RETRY")command.operation=Operation::retry;
    else return reply("ERR unknown session command\n");
    if(input>>extra)return reply("ERR unexpected command arguments\n");
    std::uint64_t ticket=0;
    if(!control_.enqueue(command,ticket))return reply("ERR command queue full or stopping\n");
    return reply("QUEUED "+std::to_string(ticket)+"\n");
  }
};
} // namespace anaiero::session

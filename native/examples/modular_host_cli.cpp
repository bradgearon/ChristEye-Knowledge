#include "anaiero/sdk/native_session.hpp"
#include "anaiero/sdk/engines.hpp"
#ifdef ANAIERO_HAS_CONTINUOUS_MESH
#include "anaiero/sdk/native_mesh.hpp"
#endif
#include "anaiero/infrastructure/native_host_lifecycle.hpp"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>

namespace s=anaiero::sdk;
namespace h=s::host;
namespace ipc=anaiero::host;
namespace fs=std::filesystem;
namespace {
struct Shutdown {
  h::NativeContinuousHost& host;
#ifdef ANAIERO_HAS_CONTINUOUS_MESH
  s::mesh::NativeServer* mesh=nullptr;
#endif
  void request()noexcept{
#ifdef ANAIERO_HAS_CONTINUOUS_MESH
    if(mesh)mesh->request_stop();
#endif
    host.request_stop();
  }
  void finish()noexcept{
#ifdef ANAIERO_HAS_CONTINUOUS_MESH
    if(mesh)mesh->stop();
#endif
    host.shutdown();
  }
  ~Shutdown(){finish();}
};
bool number(std::string_view text,std::uint64_t& value) {
  const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
  return !text.empty()&&result.ec==std::errc{}&&result.ptr==text.data()+text.size()&&std::to_string(value)==text;
}
std::string executable(const char* argv0) {
#ifdef _WIN32
  (void)argv0;std::wstring path(32768,L'\0');
  const auto count=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
  if(!count||count>=path.size())return {};
  path.resize(count);const auto utf8=fs::path(path).u8string();return {utf8.begin(),utf8.end()};
#else
  std::string path(8192,'\0');const auto count=readlink("/proc/self/exe",path.data(),path.size());
  if(count>0&&static_cast<std::size_t>(count)<path.size()) {path.resize(static_cast<std::size_t>(count));return path;}
  return fs::absolute(argv0).string();
#endif
}
ipc::Reply reply(std::string_view text,bool stop=false) {
  ipc::Reply value;if(text.empty()||text.size()>value.bytes.size())text="ERR bounded reply exceeded\n";
  std::memcpy(value.bytes.data(),text.data(),text.size());value.length=text.size();value.stop=stop;return value;
}
#ifdef ANAIERO_HAS_CONTINUOUS_MESH
std::string json_text(std::string_view value){
  std::ostringstream out;out<<'"';
  for(const unsigned char ch:value){
    if(ch=='"'||ch=='\\')out<<'\\'<<static_cast<char>(ch);
    else if(ch<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(ch)<<std::dec;
    else out<<static_cast<char>(ch);
  }
  out<<'"';return out.str();
}
struct MeshAdministration {
  struct Entry {std::uint64_t ticket=0;bool complete=false,mutation=false;s::Result result;s::Bytes response;};
  std::mutex mutex;std::deque<std::shared_ptr<Entry>> entries;std::uint64_t next_ticket=1;
};
#endif
class Commands final:public ipc::ICommandService {
  h::NativeContinuousHost& host_;
  Shutdown& shutdown_;
#ifdef ANAIERO_HAS_CONTINUOUS_MESH
  std::shared_ptr<MeshAdministration> administration_=std::make_shared<MeshAdministration>();
  ipc::Reply mesh_admin(std::string command){
    auto entry=std::make_shared<MeshAdministration::Entry>();
    std::istringstream parsed(command);std::string verb;parsed>>verb;
    entry->mutation=verb=="peer"||verb=="grant"||verb=="cancel-bootstrap";
    {
      std::lock_guard lock(administration_->mutex);
      while(administration_->entries.size()>=32){
        const auto completed=std::find_if(administration_->entries.begin(),administration_->entries.end(),[](const auto& item){return item->complete;});
        if(completed==administration_->entries.end())return reply("ERR capacity local mesh administration is full\n");
        administration_->entries.erase(completed);
      }
      if(administration_->next_ticket==std::numeric_limits<std::uint64_t>::max())return reply("ERR capacity local mesh ticket namespace exhausted\n");
      entry->ticket=administration_->next_ticket++;administration_->entries.push_back(entry);
    }
    const auto cache=administration_;
    const auto admitted=s::mesh::submit_native_administration(host_,std::move(command),[entry,cache](s::Result result,s::Bytes response){
      std::lock_guard lock(cache->mutex);entry->result=std::move(result);entry->complete=true;
      // Escaped response bytes still fit the bounded IPC reply. Oversize data
      // does not turn an already committed mutation into an uncommitted result.
      entry->response=response.size()<=2600?std::move(response):"Response exceeded the local reply bound; use the retained operation identity to inspect its result.\n";
    });
    if(!admitted){
      std::lock_guard lock(cache->mutex);cache->entries.erase(std::remove(cache->entries.begin(),cache->entries.end(),entry),cache->entries.end());
      return reply("ERR "+std::string(s::code_name(admitted.code))+" "+admitted.message+"\n");
    }
    return reply("{\"ticket\":"+std::to_string(entry->ticket)+",\"queued\":true,\"durable\":false}\n");
  }
  ipc::Reply mesh_result(std::string_view ticket_text){
    std::uint64_t ticket=0;if(!number(ticket_text,ticket))return reply("ERR MESH_RESULT <ticket>\n");
    std::lock_guard lock(administration_->mutex);
    const auto found=std::find_if(administration_->entries.begin(),administration_->entries.end(),[&](const auto& item){return item->ticket==ticket;});
    if(found==administration_->entries.end())return reply("ERR not_found local mesh result is not retained; retry using the original stable operation identity\n");
    const auto& value=**found;
    return reply("{\"ticket\":"+std::to_string(ticket)+",\"complete\":"+(value.complete?"true":"false")+
      ",\"durable\":"+(value.complete&&value.mutation&&value.result.committed()?"true":"false")+",\"code\":\""+std::string(s::code_name(value.result.code))+
      "\",\"response\":"+json_text(value.response)+"}\n");
  }
#endif
public:
  Commands(h::NativeContinuousHost& host,Shutdown& shutdown):host_(host),shutdown_(shutdown){}
  ipc::Reply handle(std::string_view text) noexcept override {
    try{return dispatch(text);}catch(...){return reply("ERR host command failed\n");}
  }
  ipc::Reply dispatch(std::string_view text) {
    if(text=="PING")return reply("PONG\n");
    if(text=="STOP") {shutdown_.request();return reply("BYE draining accepted work\n",true);}
#ifdef ANAIERO_HAS_CONTINUOUS_MESH
    if(text.starts_with("MESH "))return mesh_admin(std::string(text.substr(5)));
    if(text.starts_with("MESH_RESULT "))return mesh_result(text.substr(12));
    if(text=="MESH_STATUS"){
      if(!shutdown_.mesh)return reply("ERR mesh listener is not configured\n");
      const auto status=shutdown_.mesh->status();
      return reply("{\"schema\":\"anaiero.host-mesh-status/1\",\"port\":"+std::to_string(status.port)+
        ",\"running\":"+(status.running?"true":"false")+",\"handshakes\":"+std::to_string(status.handshakes)+
        ",\"clients\":"+std::to_string(status.clients)+",\"queued_work\":"+std::to_string(status.queued_work)+
        ",\"availability_generation\":"+std::to_string(status.availability_generation)+
        ",\"delivered_pages\":"+std::to_string(status.delivered_pages)+",\"acknowledged_pages\":"+std::to_string(status.acknowledged_pages)+
        ",\"delivered_bytes\":"+std::to_string(status.delivered_bytes)+",\"charged_read_bytes\":"+std::to_string(status.charged_read_bytes)+
        ",\"owner_wakeups\":"+std::to_string(status.owner_wakeups)+"}\n");
    }
#endif
    std::istringstream input{std::string(text)};std::string word,extra;input>>word;
    if(word=="STATUS") {
      std::uint64_t offset=0;
      if(input>>extra)if(!number(extra,offset)||offset>32)return reply("ERR STATUS [engine-offset 0..32]\n");
      if(input>>extra)return reply("ERR unexpected STATUS argument\n");
      return reply(h::encode_status(host_.snapshot(),static_cast<std::size_t>(offset)));
    }
    if(word=="RESULT") {
      std::uint64_t ticket=0;
      if(!(input>>extra)||!number(extra,ticket)||input>>extra)return reply("ERR RESULT <ticket>\n");
      h::CommandResult value;const auto result=host_.command_result(ticket,value);
      if(!result)return reply("ERR "+std::string(s::code_name(result.code))+"\n");
      return reply("{\"ticket\":"+std::to_string(ticket)+",\"complete\":"+(value.complete?"true":"false")+
        ",\"durable\":"+(value.durable?"true":"false")+",\"code\":\""+std::string(s::code_name(value.result.code))+"\"}\n");
    }
    h::Command command;
    if(word=="RUN")command.kind=h::CommandKind::run;
    else if(word=="PAUSE")command.kind=h::CommandKind::pause;
    else if(word=="THROUGH") {
      command.kind=h::CommandKind::through;
      if(!(input>>extra)||!number(extra,command.through_cycle))return reply("ERR THROUGH <absolute-cycle>\n");
    }else if(word=="ENGINE_PAUSE"||word=="ENGINE_RESUME"||word=="ENGINE_CANCEL") {
      command.kind=word=="ENGINE_PAUSE"?h::CommandKind::engine_pause:
        word=="ENGINE_RESUME"?h::CommandKind::engine_resume:h::CommandKind::engine_cancel;
      if(!(input>>command.instance_id>>command.operation_id))return reply("ERR engine-control <instance> <stable-operation-id>\n");
    }else return reply("ERR unknown command\n");
    if(input>>extra)return reply("ERR unexpected command argument\n");
    std::uint64_t ticket=0;const auto result=host_.enqueue(std::move(command),ticket);
    if(!result)return reply("ERR "+std::string(s::code_name(result.code))+"\n");
    return reply("{\"ticket\":"+std::to_string(ticket)+",\"queued\":true,\"durable\":false}\n");
  }
};
void help() {
  std::cout<<"Anaiero canonical native host\n"
    "  anaiero host start ENDPOINT STORE PERIOD_MS [paused] [--mesh CONFIG]\n"
    "  anaiero host service ENDPOINT STORE PERIOD_MS [paused] [--mesh CONFIG]\n"
    "  anaiero host query ENDPOINT COMMAND [ARGUMENTS...]\n"
    "  anaiero host check STORE\n"
    "  anaiero host package-info\n"
    "Commands: STATUS [engine-offset], PING, RUN, PAUSE, THROUGH absolute-cycle,\n"
    "  ENGINE_PAUSE|ENGINE_RESUME|ENGINE_CANCEL instance stable-operation-id,\n"
    "  RESULT ticket, STOP. Queue admission is not a durable acknowledgement.\n"
    "Optional mesh build: MESH_STATUS, MESH <local enrollment/grant command>, MESH_RESULT ticket.\n";
}
s::Result execute(int argc,char** argv) {
  if(argc<2||std::string_view(argv[1])=="help"||std::string_view(argv[1])=="--help") {help();return {};}
  const std::string_view mode=argv[1];
  if(mode=="query"&&argc>=4) {
    std::string command=argv[3];for(int i=4;i<argc;++i) {command+=' ';command+=argv[i];}
    std::string response;
    if(!ipc::NativeIpcClient::request(argv[2],command,response))return {s::Code::io_error,"local host request did not complete"};
    std::cout<<response;
    return response.starts_with("ERR")?s::Result{s::Code::invalid,"local host rejected command"}:s::Result{};
  }
  if(mode=="package-info"&&argc==2) {
    s::EngineModuleCatalog catalog;if(auto result=anaiero::engines::register_builtin_engines(catalog);!result)return result;
    std::cout<<"{\"schema\":\"anaiero.host-package/1\",\"module_sha256\":\""<<catalog.describe().front().module_sha256
      <<"\",\"store_format\":\"canonical-root\",\"native_ipc\":1,\"transparent_live_upgrade\":false}\n";return {};
  }
  if(mode=="check"&&argc==3) {
    h::NativeSession session;if(auto result=session.open(argv[2]);!result)return result;
    std::cout<<"{\"schema\":\"anaiero.host-compatibility/1\",\"compatible\":true,\"generation\":"<<session.store().generation()
      <<",\"completed_cycles\":"<<session.runner().progress().completed_cycles
      <<",\"cycle_pending\":"<<(session.runner().progress().cycle_pending?"true":"false")<<"}\n";return {};
  }
  if((mode!="start"&&mode!="service")||argc<5||argc>8)return {s::Code::invalid,"unknown host command or argument count; use help"};
  std::uint64_t period=0;
  if(!number(argv[4],period)||period==0||period>3600000)return {s::Code::invalid,"period must be 1..3600000 ms"};
  bool paused=false;std::string mesh_configuration;
  for(int argument=5;argument<argc;++argument){
    if(std::string_view(argv[argument])=="paused"&&!paused)paused=true;
    else if(std::string_view(argv[argument])=="--mesh"&&mesh_configuration.empty()&&argument+1<argc)mesh_configuration=argv[++argument];
    else return {s::Code::invalid,"optional host arguments are paused and --mesh CONFIG"};
  }
#ifdef ANAIERO_HAS_CONTINUOUS_MESH
  s::mesh::NativeServerOptions mesh_options;
  if(!mesh_configuration.empty())if(auto result=s::mesh::read_native_server_options(mesh_configuration,mesh_options);!result)return result;
#else
  if(!mesh_configuration.empty())return {s::Code::unsupported,"configure ANAIERO_ENABLE_MESH=ON to use an authenticated listener"};
#endif
  h::NativeSession session;if(auto result=session.open(argv[3]);!result)return result;
  if(auto result=session.ensure_engines();!result)return result;
  if(auto result=session.retain_host_artifact(executable(argv[0]));!result)return result;
  h::Options options;options.period_ms=static_cast<std::uint32_t>(period);options.initially_running=!paused;
  h::NativeContinuousHost worker(session.runtime(),session.runner(),session.store(),options);
#ifdef ANAIERO_HAS_CONTINUOUS_MESH
  std::unique_ptr<s::mesh::NativeServer> mesh_server;
#endif
  Shutdown shutdown{worker};
  if(auto result=worker.start();!result)return result;
#ifdef ANAIERO_HAS_CONTINUOUS_MESH
  if(!mesh_configuration.empty()){
    mesh_server=std::make_unique<s::mesh::NativeServer>(worker,std::move(mesh_options));shutdown.mesh=mesh_server.get();
    worker.set_commit_wakeup([&](std::uint64_t generation){mesh_server->notify_commit(generation);});
    if(auto result=mesh_server->start();!result)return result;
    mesh_server->notify_commit(worker.snapshot().store.generation);
  }
#endif
  Commands commands(worker,shutdown);ipc::NativeIpcServer server(argv[2]);
  ipc::NativeLifecycleOptions lifecycle;lifecycle.windows_service=mode=="service";lifecycle.service_name="AnaieroModular";
  lifecycle.on_stop=[](void* context)noexcept{static_cast<Shutdown*>(context)->finish();};
  lifecycle.stop_context=&shutdown;
  const auto result=ipc::NativeHostLifecycle::run(server,commands,lifecycle);
  shutdown.finish();
  if(result!=ipc::Code::ok&&result!=ipc::Code::interrupted)return {s::Code::io_error,"native local host lifecycle failed"};
  return worker.snapshot().result;
}
}
int main(int argc,char** argv) {
  try {
    const auto result=execute(argc,argv);
    if(!result)std::cerr<<"anaiero host: "<<s::code_name(result.code)<<": "<<result.message<<'\n';
    return result?0:1;
  }catch(const std::exception& error) {std::cerr<<"anaiero host: "<<error.what()<<'\n';return 1;}
}

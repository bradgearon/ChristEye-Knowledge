#include "mixed_qualification.hpp"
#include "anaiero/sdk/engine_files.hpp"
#include "anaiero/sdk/native_session.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif
namespace q=anaiero::qualification;
namespace s=anaiero::sdk;
namespace e=anaiero::engines;
namespace ev=s::evidence;
namespace fs=std::filesystem;
#define CHECK(condition) do{if(!(condition)){std::cerr<<"FAILED line "<<__LINE__<<": " #condition "\n";std::exit(1);}}while(false)
void require(const s::Result& r){if(!r){std::cerr<<s::code_name(r.code)<<": "<<r.message<<'\n';std::exit(1);}}
std::string read(const fs::path& path){std::ifstream input(path,std::ios::binary);CHECK(input.good());std::ostringstream out;out<<input.rdbuf();return out.str();}
void write(const fs::path& path,std::string_view bytes){std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<bytes;CHECK(out.good());}
struct Input {
  std::string profile,text;s::SessionDefinition definition;
  explicit Input(const fs::path& repository){
    profile=read(repository/"qualification/native/mixed-campaign-ci.cevqualification");
    text=read(repository/"examples/runtime/modular-native.cevsession");
    s::EngineModuleCatalog catalog;require(e::register_builtin_engines(catalog));require(catalog.freeze());
    require(e::read_session_definition((repository/"examples/runtime/modular-native.cevsession").string(),catalog,definition));
  }
  void create(const fs::path& path){require(q::create_mixed_campaign(profile,text,definition,path.string()));}
};
struct Cut final:s::IObserverSink {
  s::Result enqueue(const s::ObserverEvent& event) override {
    if(event.kind==s::ObserverEventKind::committed&&event.tick==1)std::_Exit(73);
    return {};
  }
};
int child(const fs::path& path,std::string mode){
  q::CampaignRunOptions options;Cut cut;
  if(mode=="engine")options.engine_observer=&cut;
  else if(mode=="publication")options.observe=[](q::CampaignEvent event,std::uint64_t){
    if(event==q::CampaignEvent::completion_committed)std::_Exit(73);
  };
  else options.observe=[](q::CampaignEvent event,std::uint64_t cycles){
    if(event==q::CampaignEvent::work_completed&&cycles==10)std::_Exit(73);
  };
  q::MixedReport report;(void)q::run_mixed_campaign(path.string(),report,options);return 74;
}
void process_cut(const fs::path& executable,const fs::path& directory,std::string mode){
#ifdef _WIN32
  const auto quote=[](std::wstring_view text){std::wstring out=L"\"";std::size_t slashes=0;
    for(const auto c:text){if(c==L'\\'){++slashes;continue;}out.append(c==L'"'?slashes*2+1:slashes,L'\\');out+=c;slashes=0;}
    out.append(slashes*2,L'\\');out+=L'"';return out;};
  const auto exe=fs::absolute(executable).wstring();
  auto command=quote(exe)+L" --cut "+quote(fs::absolute(directory).wstring())+L" "+std::wstring(mode.begin(),mode.end());
  STARTUPINFOW startup{};startup.cb=static_cast<DWORD>(sizeof(startup));PROCESS_INFORMATION process{};
  CHECK(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process));
  CloseHandle(process.hThread);CHECK(WaitForSingleObject(process.hProcess,90000)==WAIT_OBJECT_0);
  DWORD code=0;CHECK(GetExitCodeProcess(process.hProcess,&code));CloseHandle(process.hProcess);CHECK(code==73);
#else
  std::vector<std::string> owned{fs::absolute(executable).string(),"--cut",fs::absolute(directory).string(),std::move(mode)};
  std::vector<char*> args;for(auto& arg:owned)args.push_back(arg.data());args.push_back(nullptr);
  pid_t process=-1;CHECK(posix_spawn(&process,args.front(),nullptr,nullptr,args.data(),environ)==0);
  int code=0;pid_t done=-1;do{done=waitpid(process,&code,0);}while(done<0&&errno==EINTR);
  CHECK(done==process&&WIFEXITED(code)&&WEXITSTATUS(code)==73);
#endif
}
void completed_state(const fs::path& path){
  s::host::NativeSession session;require(session.open((path/"store").string()));require(session.ensure_engines());
  CHECK(session.runner().progress().completed_cycles==32&&!session.runner().progress().cycle_pending);
  CHECK(session.runtime().find("cells")->state().tick==32);
  CHECK(session.runtime().find("colonies")->state().tick==16);
  CHECK(session.runtime().find("program")->state().tick==12&&session.runtime().find("program")->state().phase==s::Phase::completed);
  CHECK(session.runtime().find("voltage")->state().tick==32&&session.runtime().find("environment")->state().tick==30);
  ev::AuditReport audit;require(session.store().audit(audit));
}
void normal(Input& input,const fs::path& path){
  input.create(path);q::MixedReport report;q::CampaignRunOptions options;bool exclusion=false,status_read=false;
  options.observe=[&](q::CampaignEvent event,std::uint64_t){if(event==q::CampaignEvent::attempt_started){
    q::MixedReport concurrent;CHECK(q::run_mixed_campaign(path.string(),concurrent).code==s::Code::busy);exclusion=true;
    std::string cached;require(q::mixed_campaign_status(path.string(),cached));CHECK(cached.find("\"cached\":true")!=std::string::npos);status_read=true;
  }};
  require(q::run_mixed_campaign(path.string(),report,options));
  CHECK(exclusion&&status_read&&report.passed&&report.campaign_complete&&report.continuous_qualified&&report.campaign_attempt==1);
  CHECK(report.campaign_interruptions==0&&report.active_ns_lower_bound>0&&report.workload_ns>0);
  CHECK(report.dispatch.count==122&&report.queue.count==122&&report.step_commit.count==122&&report.projection.count==122);
  CHECK(report.dispatch_limit.passes(99)&&report.queue_limit.passes(99)&&report.step_commit_limit.passes(99)&&report.projection_limit.passes(99));
  const auto preserved=read(path/"report.json");q::MixedReport repeated;
  require(q::run_mixed_campaign(path.string(),repeated));CHECK(q::mixed_report_json(repeated)==preserved);
  CHECK(q::create_mixed_campaign(input.profile,input.text,input.definition,path.string()).code==s::Code::conflict);
  CHECK(read(path/"report.json")==preserved);completed_state(path);
}
void resume(Input& input,const fs::path& executable,const fs::path& path,const std::string& mode){
  input.create(path);process_cut(executable,path,mode);
  {
    s::host::NativeSession session;require(session.open((path/"store").string()));
    if(mode=="engine")CHECK(session.runner().progress().completed_cycles==0&&session.runner().progress().cycle_pending);
    else CHECK(session.runner().progress().completed_cycles==10&&!session.runner().progress().cycle_pending);
  }
  q::MixedReport report;const auto result=q::run_mixed_campaign(path.string(),report);
  CHECK(result.code==s::Code::capacity&&report.campaign_complete&&!report.passed&&!report.continuous_qualified);
  CHECK(report.campaign_attempt==2&&report.campaign_interruptions==1);
  if(mode=="work")CHECK(report.recovered_cycles==8);
  CHECK(fs::exists(path/"attempt-1-interrupted.json"));CHECK(fs::exists(path/"report.json"));completed_state(path);
}
void tampered(Input& input,const fs::path& path){
  input.create(path);const auto original=read(path/"profile.cevqualification");auto changed=original;
  const auto at=changed.find("period_ms=2");CHECK(at!=std::string::npos);changed.replace(at,11,"period_ms=3");write(path/"profile.cevqualification",changed);
  q::MixedReport report;CHECK(q::run_mixed_campaign(path.string(),report).code==s::Code::conflict);
  s::host::NativeSession session;require(session.open((path/"store").string()));CHECK(session.runner().progress().completed_cycles==0);
  ev::StoredRecord state;require(session.store().latest("qualification.campaign","main",state));std::vector<std::string> fields;
  require(s::unpack(state.record.payload,fields,20,16384));CHECK(fields[5]=="0");
}
void publication(Input& input,const fs::path& executable,const fs::path& path){
  input.create(path);process_cut(executable,path,"publication");CHECK(!fs::exists(path/"report.json"));
  q::MixedReport report;require(q::run_mixed_campaign(path.string(),report));
  CHECK(report.campaign_attempt==1&&report.passed&&report.continuous_qualified&&report.campaign_complete);
  CHECK(read(path/"report.json")==q::mixed_report_json(report));completed_state(path);
}
void device_pinning(Input& input,const fs::path& root){
  auto elapsed=input.profile;const auto offset=elapsed.find("minimum_run_ms=0");CHECK(offset!=std::string::npos);
  elapsed.replace(offset,16,"minimum_run_ms=1");
  CHECK(q::create_mixed_campaign(elapsed,input.text,input.definition,(root/"absent-device").string()).code==s::Code::invalid);
  CHECK(!fs::exists(root/"absent-device"));
  const std::string declaration="ANAIERODEVICE/1\nname=campaign-test\ndevice_class=shared\nmachine=fixture\nprocessor=fixture\nmemory_bytes=1048576\nstorage_device=fixture\nstorage_firmware=fixture\nstorage_controller=fixture\nfilesystem=fixture\nmount_options=fixture\ndurability=fixture\ndependencies=fixture\nconcurrent_work=fixture\n";
  auto dedicated=elapsed;const auto field=dedicated.find("required_device_class=any");CHECK(field!=std::string::npos);
  dedicated.replace(field,std::string_view("required_device_class=any").size(),"required_device_class=dedicated");
  CHECK(q::create_mixed_campaign(dedicated,input.text,input.definition,(root/"wrong-device").string(),declaration).code==s::Code::invalid);
  CHECK(!fs::exists(root/"wrong-device"));
  const auto path=root/"device";require(q::create_mixed_campaign(elapsed,input.text,input.definition,path.string(),declaration));
  CHECK(read(path/"device.cevdevice")==declaration);
  write(path/"device.cevdevice",declaration+"# different exact bytes\n");
  q::MixedReport report;CHECK(q::run_mixed_campaign(path.string(),report).code==s::Code::conflict);
  std::string cached;require(q::mixed_campaign_status(path.string(),cached));CHECK(cached.find(s::sha256(declaration))!=std::string::npos);
}
int main(int argc,char** argv){
  if(argc==4&&std::string_view(argv[1])=="--cut")return child(argv[2],argv[3]);
  CHECK(argc==3);const fs::path repository=argv[1],root=argv[2];fs::remove_all(root);fs::create_directories(root);Input input(repository);
  normal(input,root/"normal");resume(input,argv[0],root/"work-cut","work");resume(input,argv[0],root/"engine-cut","engine");
  publication(input,argv[0],root/"publication-cut");tampered(input,root/"tamper");device_pinning(input,root);
  std::cout<<"mixed campaign: exact pins, canonical targets, writer exclusion, immutable evidence, process cuts and conservative duration coverage passed\n";
}

// A real mixed-engine session, durable evidence, optimizer route lifecycle and
// process-death recovery. No sockets or separately installed solvers required.
#include "anaiero/application/native_session_application.hpp"
#include "anaiero/infrastructure/epoch_observation_codec.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define CHECK(x) do {if(!(x)){std::cerr<<"FAILED " #x " at "<<__LINE__<<'\n';std::abort();}}while(false)
namespace app=anaiero::application;
namespace session=anaiero::session;
namespace engine=anaiero::engine;
namespace runtime=anaiero::runtime;
namespace db=christeye::living::durable;
namespace fs=std::filesystem;

static constexpr std::array<runtime::Metric,5> metrics{{runtime::Metric::viability,
  runtime::Metric::energy,runtime::Metric::connection,runtime::Metric::digital_learning,
  runtime::Metric::digital_route_active}};

static std::unique_ptr<app::NativeSessionApplication> open_session(
    const fs::path& config,const fs::path& root,runtime::IAsyncCompletionSignal* signal=nullptr) {
  auto result=std::make_unique<app::NativeSessionApplication>(signal);
  const bool opened=result->open(config,root);
  if(!opened)std::cerr<<"session open: "<<result->error()<<'\n';
  CHECK(opened);
  return result;
}
static void settle(app::NativeSessionApplication& value) {
  const bool done=value.settle();
  if(!done)std::cerr<<value.service().state_json();
  CHECK(done&&!value.service().busy()&&!value.service().failed()&&value.service().quiescent());
  CHECK(!value.service().record().publication.active&&!value.service().record().change.active);
}
static bool same_snapshot(const engine::Snapshot& a,const engine::Snapshot& b) {
  return a.id==b.id&&a.source_epoch==b.source_epoch&&a.tick==b.tick&&
    a.revision==b.revision&&a.participants==b.participants&&a.connections==b.connections&&
    a.energy==b.energy&&a.learning_proxy==b.learning_proxy&&a.state_sha256==b.state_sha256;
}
static std::array<std::int64_t,5> metric_values(const session::EngineEvidence& e) {
  return {{e.published.participants,e.published.energy,e.published.connections,
    e.published.learning_proxy,e.route.active?1:0}};
}
static void verify_visible_metrics(app::NativeSessionApplication& value) {
  const auto json=value.service().state_json();
  CHECK(json.find("\"sequence_gaps\":0")!=std::string::npos);
  CHECK(json.find("\"pending_evidence\":0")!=std::string::npos);
  for(std::size_t i=0;i<value.configuration().count;++i) {
    const auto& cfg=value.configuration().engines[i];
    const auto& state=value.service().record().evidence[i];
    const auto expected=metric_values(state);
    CHECK(same_snapshot(state.published,value.service().record().coordinator.engines[i].last));
    for(std::size_t m=0;m<metrics.size();++m) {
      std::ostringstream text;
      text<<"\"engine\":"<<cfg.id<<",\"metric\":"<<static_cast<unsigned>(metrics[m])
          <<",\"value\":"<<expected[m]<<",\"source\":"<<cfg.source<<",\"epoch\":"<<cfg.epoch;
      CHECK(json.find(text.str())!=std::string::npos);
    }
  }
}
// Independently open the on-disk journal after the application releases its
// writer lock, decode every payload and require complete consecutive groups.
static void verify_journal(const fs::path& root,const session::Configuration& cfg,
                           const session::Record& record,std::size_t expected_count) {
  db::Registry journal;CHECK(journal.open(root/"evidence")==db::Code::ok);
  CHECK(journal.event_count()==expected_count&&journal.audit_disk());
  runtime::EpochObservationCodec codec;
  std::map<std::uint32_t,std::vector<runtime::Observation>> streams;
  for(const auto& event:journal.events()) {
    CHECK(event.kind=="OBSERVATION_RECORDED"&&event.entity==cfg.name&&event.origin=="SIMULATED");
    std::string wire;CHECK(journal.get(event.object_hash,wire));
    runtime::Observation decoded{};
    CHECK(codec.decode(std::span<const std::byte>(reinterpret_cast<const std::byte*>(wire.data()),wire.size()),decoded));
    const session::EngineConfig* source=nullptr;
    for(std::size_t i=0;i<cfg.count;++i)
      if(cfg.engines[i].source==decoded.source)source=&cfg.engines[i];
    CHECK(source&&decoded.source_epoch==source->epoch&&decoded.colony==source->id);
    CHECK(event.source_tick==decoded.sequence&&event.clock=="epoch-"+std::to_string(source->epoch)&&
      event.producer=="source-"+std::to_string(source->source));
    auto& stream=streams[decoded.source];
    CHECK(decoded.sequence==stream.size()+1); // no missing, duplicate or reordered event
    CHECK(decoded.metric==metrics[stream.size()%metrics.size()]);
    if(!stream.empty()&&stream.size()%metrics.size()!=0) {
      CHECK(decoded.acquired_ns==stream.back().acquired_ns&&decoded.evidence_tag==stream.back().evidence_tag);
    }else if(!stream.empty())CHECK(decoded.acquired_ns>=stream.back().acquired_ns);
    stream.push_back(decoded);
  }
  std::size_t total=0;
  for(std::size_t i=0;i<cfg.count;++i) {
    const auto& stream=streams[cfg.engines[i].source];const auto& evidence=record.evidence[i];
    CHECK(stream.size()>=5&&stream.size()%5==0&&stream.size()+1==evidence.next_sequence);
    const auto values=metric_values(evidence);
    for(std::size_t m=0;m<metrics.size();++m) {
      const auto& observed=stream[stream.size()-5+m];
      CHECK(observed.value==values[m]&&observed.acquired_ns==evidence.published.tick*1000000ULL);
    }
    total+=stream.size();
  }
  CHECK(total==expected_count);
}
static void step(app::NativeSessionApplication& value,std::uint32_t count) {
  CHECK(value.service().execute({session::Operation::step,0,0,0,count},nullptr));
  settle(value);verify_visible_metrics(value);
}
static void analyze(app::NativeSessionApplication& value) {
  CHECK(value.service().execute({session::Operation::analyze,101,3,4,18},nullptr));
  if(!value.service().record().proposal.selection.approved)std::cerr<<value.service().state_json();
  CHECK(value.service().record().proposal.selection.approved);
  CHECK(value.service().record().proposal.selection.forecast.baseline.tick==
        value.service().record().proposal.selection.forecast.trial.tick);
  CHECK(value.service().record().proposal.selection.forecast.source_state_sha256==
        value.service().record().coordinator.engines[0].last.state_sha256.data());
}
static void write_changed_config(const fs::path& path,const session::Configuration& cfg,
                                 bool alter_period=true) {
  std::ofstream out(path,std::ios::binary);CHECK(out.good());
  const auto period=alter_period?(cfg.period_ms==60000?59999:cfg.period_ms+1):cfg.period_ms;
  out<<"CEVSESSION/1\nname "<<cfg.name<<"\nperiod_ms "<<period<<'\n';
  for(std::size_t i=0;i<cfg.count;++i) {
    const auto& e=cfg.engines[i];
    out<<"engine "<<e.id<<' '<<(e.kind==engine::Kind::sw5?"sw5":"sim3")<<' '
       <<e.source<<' '<<e.epoch<<' '<<static_cast<unsigned>(e.cadence)<<' '
       <<std::quoted(e.model_path)<<' '<<std::quoted(e.initial_path)<<' '
       <<e.inputs.support_proxy<<' '<<e.inputs.stress_proxy<<' '<<e.inputs.digital_signal<<' '<<e.inputs.lesson<<'\n';
  }
  out<<"end\n";out.close();CHECK(!out.fail());
}
struct Cancelled final:session::ICancellation {
  bool cancelled()const noexcept override{return true;}
};
struct ExitAfterCommit final:runtime::IAsyncCompletionSignal {
  std::atomic<bool> armed=false;
  void notify()noexcept override {
    if(armed.load(std::memory_order_acquire))std::_Exit(73);
  }
};
struct ExitAfterRouteIntent final:anaiero::contracts::IServiceExtension {
  session::NativeSessionService* owner=nullptr;
  bool armed=false;
  void on_service_event(const anaiero::contracts::ServiceEvent& e)noexcept override {
    if(armed&&owner&&e.service==anaiero::contracts::ServiceId::runtime_dispatch&&
       e.phase==anaiero::contracts::ServicePhase::committed&&owner->record().change.active)
      std::_Exit(74);
  }
};
static int crash_child(const fs::path& config,const fs::path& root) {
  ExitAfterCommit crash;
  auto value=open_session(config,root,&crash);settle(*value);
  crash.armed.store(true,std::memory_order_release);
  CHECK(value->service().execute({session::Operation::step,0,0,0,1},nullptr));
  settle(*value);
  return 74; // observer must terminate the actual process after durable commit
}
static int route_crash_child(const fs::path& config,const fs::path& root) {
  ExitAfterRouteIntent crash;
  auto value=open_session(config,root);settle(*value);analyze(*value);
  crash.owner=&value->service();crash.armed=true;value->service().set_extension(&crash);
  CHECK(value->service().execute({session::Operation::apply,101},nullptr));
  return 75; // the durable intent hook must exit before engine replacement
}
#ifdef _WIN32
static std::wstring quote_windows(std::wstring_view arg) {
  std::wstring out=L"\"";std::size_t slashes=0;
  for(wchar_t ch:arg) {
    if(ch==L'\\'){++slashes;continue;}
    if(ch==L'\"'){out.append(slashes*2+1,L'\\');out+=ch;slashes=0;continue;}
    out.append(slashes,L'\\');slashes=0;out+=ch;
  }
  out.append(slashes*2,L'\\');out+=L'\"';return out;
}
#endif
static void invoke_crash_child(const fs::path& executable,const fs::path& config,const fs::path& root,
                               bool route_intent=false) {
  const int expected=route_intent?74:73;
#ifdef _WIN32
  const auto exe=fs::absolute(executable).wstring();
  const std::wstring mode=route_intent?L" --crash-after-route ":L" --crash-after-evidence ";
  std::wstring command=quote_windows(exe)+mode+
    quote_windows(fs::absolute(config).wstring())+L" "+quote_windows(fs::absolute(root).wstring());
  STARTUPINFOW startup{};startup.cb=static_cast<DWORD>(sizeof(startup));PROCESS_INFORMATION process{};
  CHECK(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process));
  CloseHandle(process.hThread);
  const auto waited=WaitForSingleObject(process.hProcess,60000);
  if(waited!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,75);CloseHandle(process.hProcess);CHECK(false);}
  DWORD result=0;CHECK(GetExitCodeProcess(process.hProcess,&result));CloseHandle(process.hProcess);
  CHECK(result==static_cast<DWORD>(expected));
#else
  const auto exe=fs::absolute(executable).string();const auto cfg=fs::absolute(config).string();
  const auto target=fs::absolute(root).string();
  const auto child=fork();CHECK(child>=0);
  if(child==0){
    const char* mode=route_intent?"--crash-after-route":"--crash-after-evidence";
    execl(exe.c_str(),exe.c_str(),mode,cfg.c_str(),target.c_str(),static_cast<char*>(nullptr));
    std::_Exit(76);
  }
  int status=0;pid_t done=0;
  do{done=waitpid(child,&status,0);}while(done<0&&errno==EINTR);
  CHECK(done==child&&WIFEXITED(status)&&WEXITSTATUS(status)==expected);
#endif
}
int main(int argc,char** argv) {
  if(argc==4&&std::string_view(argv[1])=="--crash-after-evidence")return crash_child(argv[2],argv[3]);
  if(argc==4&&std::string_view(argv[1])=="--crash-after-route")return route_crash_child(argv[2],argv[3]);
  CHECK(argc==3);
  const auto config=fs::absolute(argv[1]);const auto root=fs::absolute(argv[2]);
  fs::create_directories(root);
  const auto mixed=root/"mixed",stale=root/"stale",crashed=root/"crashed",route_crashed=root/"route-crashed";
  fs::remove_all(mixed);fs::remove_all(stale);fs::remove_all(crashed);fs::remove_all(route_crashed);
  session::Configuration pinned{};session::Record checkpoint{},final_state{};
  {
    auto value=open_session(config,mixed);settle(*value);pinned=value->configuration();
    CHECK(pinned.count==2&&pinned.engines[0].id==101&&pinned.engines[0].kind==engine::Kind::sw5);
    CHECK(pinned.engines[0].cadence==1&&pinned.engines[1].kind==engine::Kind::sim3&&pinned.engines[1].cadence==2);
    const auto& initial=value->service().record();
    CHECK(initial.coordinator.completed_cycles==0&&initial.evidence[0].published.tick==0);
    CHECK(initial.evidence[0].published.participants==2&&initial.evidence[0].published.energy==200);
    CHECK(initial.evidence[1].published.participants>0&&initial.evidence[1].published.energy>0);
    CHECK(initial.evidence[0].next_sequence==6&&initial.evidence[1].next_sequence==6);
    verify_visible_metrics(*value);step(*value,4);
    checkpoint=value->service().record();
    CHECK(checkpoint.coordinator.completed_cycles==4&&checkpoint.coordinator.engines[0].last.tick==4&&
      checkpoint.coordinator.engines[1].last.tick==2);
    CHECK(checkpoint.evidence[0].next_sequence==26&&checkpoint.evidence[1].next_sequence==16);
    CHECK(value->committed_observations_after_shutdown()==40);
  }
  verify_journal(mixed,pinned,checkpoint,40);
  {
    auto value=open_session(config,mixed);settle(*value);
    const auto& recovered=value->service().record();
    CHECK(recovered.revision==checkpoint.revision&&recovered.coordinator.completed_cycles==4);
    for(std::size_t i=0;i<2;++i) {
      CHECK(same_snapshot(recovered.coordinator.engines[i].last,checkpoint.coordinator.engines[i].last));
      CHECK(same_snapshot(recovered.evidence[i].published,checkpoint.evidence[i].published));
      CHECK(recovered.evidence[i].next_sequence==checkpoint.evidence[i].next_sequence);
    }
    verify_visible_metrics(*value);analyze(*value);
    const auto before_apply=value->service().record().coordinator.engines[0].last;
    CHECK(value->service().execute({session::Operation::apply,101},nullptr));settle(*value);
    const auto& applied=value->service().record();
    CHECK(applied.evidence[0].route.active&&applied.proposal.selection.commit_id!=0);
    CHECK(applied.coordinator.engines[0].last.tick==before_apply.tick&&
      applied.coordinator.engines[0].last.revision>before_apply.revision);
    CHECK(applied.coordinator.engines[0].last.state_sha256!=before_apply.state_sha256);
    verify_visible_metrics(*value);step(*value,2);
    const auto before_revert=value->service().record().coordinator.engines[0].last;
    CHECK(value->service().execute({session::Operation::revert,101},nullptr));settle(*value);
    final_state=value->service().record();
    CHECK(!final_state.evidence[0].route.active&&final_state.coordinator.completed_cycles==6);
    CHECK(final_state.coordinator.engines[0].last.tick==before_revert.tick&&before_revert.tick==6);
    CHECK(final_state.coordinator.engines[0].last.revision>before_revert.revision);
    CHECK(final_state.coordinator.engines[1].last.tick==3);
    verify_visible_metrics(*value);
    CHECK(value->committed_observations_after_shutdown()==65);
  }
  verify_journal(mixed,pinned,final_state,65);
  {
    auto value=open_session(config,mixed);settle(*value);
    CHECK(value->service().record().revision==final_state.revision);
    CHECK(!value->service().record().evidence[0].route.active);
    CHECK(same_snapshot(value->service().record().coordinator.engines[0].last,final_state.coordinator.engines[0].last));
    verify_visible_metrics(*value);CHECK(value->committed_observations_after_shutdown()==65);
  }
  const auto changed=root/"changed.cevsession";write_changed_config(changed,pinned);
  {
    auto value=std::make_unique<app::NativeSessionApplication>();
    CHECK(!value->open(changed,mixed));
    CHECK(value->error().find("identity changed")!=std::string::npos);
  }
  {
    // Renaming both the session and engine/source identities must not turn an
    // occupied directory into a fresh session or append new checkpoints.
    auto renamed=pinned;renamed.name+="-renamed";
    for(std::size_t i=0;i<renamed.count;++i){renamed.engines[i].id+=1000;renamed.engines[i].source+=1000;}
    const auto renamed_path=root/"renamed.cevsession";write_changed_config(renamed_path,renamed,false);
    std::size_t metadata_count=0;std::string metadata_tip;
    {
      db::Registry metadata;CHECK(metadata.open(mixed/"session")==db::Code::ok);
      metadata_count=metadata.event_count();CHECK(metadata_count>0);
      metadata_tip=metadata.events().back().hash;
    }
    {
      auto value=std::make_unique<app::NativeSessionApplication>();
      CHECK(!value->open(renamed_path,mixed));
      CHECK(value->error().find("different session")!=std::string::npos);
    }
    {
      db::Registry metadata;CHECK(metadata.open(mixed/"session")==db::Code::ok);
      CHECK(metadata.event_count()==metadata_count&&metadata.events().back().hash==metadata_tip&&metadata.audit_disk());
    }
    for(std::size_t i=0;i<renamed.count;++i)
      CHECK(!fs::exists(mixed/("engine-"+std::to_string(renamed.engines[i].id))));
  }
  verify_journal(mixed,pinned,final_state,65);
  {
    auto value=open_session(config,stale);settle(*value);analyze(*value);
    step(*value,1);const auto before=value->service().record();
    CHECK(!value->service().execute({session::Operation::apply,101},nullptr));
    CHECK(!value->service().failed()&&!value->service().record().evidence[0].route.active);
    CHECK(value->service().record().revision==before.revision&&value->service().record().changes==0);
    Cancelled cancel;
    CHECK(!value->service().execute({session::Operation::analyze,101,3,4,18},&cancel));
    CHECK(value->service().record().revision==before.revision);
    final_state=value->service().record();CHECK(value->committed_observations_after_shutdown()==15);
  }
  verify_journal(stale,pinned,final_state,15);
  invoke_crash_child(argv[0],config,crashed);
  {
    // The child died after committing the first metric of the next snapshot,
    // before it could finish the five-metric group or shut down cleanly.
    db::Registry interrupted;
    CHECK(interrupted.open(crashed/"evidence")==db::Code::ok);
    CHECK(interrupted.event_count()==11&&interrupted.audit_disk());
  }
  {
    auto value=open_session(config,crashed);settle(*value);final_state=value->service().record();
    CHECK(final_state.coordinator.completed_cycles==1&&!final_state.coordinator.cycle_active);
    CHECK(final_state.coordinator.engines[0].last.tick==1&&final_state.coordinator.engines[1].last.tick==0);
    CHECK(final_state.evidence[0].next_sequence==11&&final_state.evidence[1].next_sequence==6);
    verify_visible_metrics(*value);CHECK(value->committed_observations_after_shutdown()==15);
  }
  verify_journal(crashed,pinned,final_state,15);
  invoke_crash_child(argv[0],config,route_crashed,true);
  {
    db::Registry interrupted;
    CHECK(interrupted.open(route_crashed/"evidence")==db::Code::ok);
    CHECK(interrupted.event_count()==10&&interrupted.audit_disk());
    db::Registry metadata;CHECK(metadata.open(route_crashed/"session")==db::Code::ok);
    session::files::WindStoreRecordStore records(metadata,"session-"+pinned.name);
    session::Record intent{};bool exists=false;CHECK(records.load(intent,exists)&&exists);
    CHECK(intent.change.active&&intent.evidence[0].route.active&&intent.changes==1);
    CHECK(intent.proposal.selection.commit_id==1&&intent.coordinator.completed_cycles==0);
    CHECK(intent.coordinator.engines[0].last.tick==0&&
      intent.change.source_sha256==intent.coordinator.engines[0].last.state_sha256.data());
    engine::WindStoreCheckpointCatalog catalog(metadata);
    engine::CheckpointRecord old{};std::string bytes;
    CHECK(catalog.latest(pinned.engines[0].id,pinned.engines[0].epoch,old,bytes));
    CHECK(old.checkpoint_sha256==intent.change.source_sha256&&bytes!=intent.change.target_checkpoint);
  }
  {
    auto value=open_session(config,route_crashed);settle(*value);final_state=value->service().record();
    CHECK(final_state.coordinator.completed_cycles==0&&final_state.coordinator.engines[0].last.tick==0);
    CHECK(final_state.evidence[0].route.active&&final_state.evidence[0].route.change_id==1);
    CHECK(final_state.changes==1&&final_state.proposal.selection.commit_id==1&&!final_state.change.active);
    verify_visible_metrics(*value);CHECK(value->committed_observations_after_shutdown()==15);
  }
  verify_journal(route_crashed,pinned,final_state,15);
  {
    auto value=open_session(config,route_crashed);settle(*value);
    const auto& replayed=value->service().record();
    CHECK(replayed.revision==final_state.revision&&replayed.changes==1&&replayed.evidence[0].route.change_id==1);
    CHECK(replayed.evidence[0].route.active&&replayed.proposal.selection.commit_id==1);
    CHECK(same_snapshot(replayed.coordinator.engines[0].last,final_state.coordinator.engines[0].last));
    CHECK(replayed.evidence[0].next_sequence==final_state.evidence[0].next_sequence);
    CHECK(value->committed_observations_after_shutdown()==15);
  }
  std::cout<<"native mixed sessions, durable replay, generic optimization, stale rejection and process-crash recovery PASS\n";
}

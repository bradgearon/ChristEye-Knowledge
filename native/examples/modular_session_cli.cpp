// Composition and operator presentation for the compiled modular runtime.
#include "anaiero/sdk/native_session.hpp"
#include "modular_session_io.hpp"
#include "anaiero/sdk/engine_files.hpp"
#include "anaiero/sdk/evidence.hpp"
#include "anaiero/sdk/migration_receipts.hpp"
#include "anaiero/sdk/experiments.hpp"
#include "anaiero/sdk/experiment_history.hpp"
#include "anaiero/sdk/lifecycle.hpp"
#include "anaiero/sdk/session_runner.hpp"
#include "anaiero/sdk/storage_tools.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace s=anaiero::sdk;
namespace e=anaiero::engines;
namespace ev=anaiero::sdk::evidence;
namespace life=anaiero::sdk::lifecycle;
namespace fs=std::filesystem;
namespace {
using anaiero::operator_io::read_file;using anaiero::operator_io::stored_budget;
using anaiero::operator_io::canonical_root_exists;using anaiero::operator_io::now;
using anaiero::operator_io::executable;
s::Result invalid(std::string message){return {s::Code::invalid,std::move(message)};
}
std::string json(std::string_view text){
  std::ostringstream out;
  out<<'"';
  for(const unsigned char c:text){
    if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);
    else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;
    else out<<static_cast<char>(c);
  }
  out<<'"';
  return out.str();
}
std::string_view index_layout_label(std::string_view backend) {
  if (backend == "native-sealed-avl") return "node-files";
  if (backend == "native-packed-pages/1") return "packed-pages";
  return backend;
}

std::string_view target_layout_label(ev::IndexLayout layout) {
  switch (layout) {
    case ev::IndexLayout::single_node: return "node-files";
    case ev::IndexLayout::packed_pages: return "packed-pages";
    case ev::IndexLayout::automatic: return "preserve";
  }
  return "unknown";
}

s::Result parse_index_layout(std::string_view text, ev::IndexLayout& layout) {
  if (text == "node-files") layout = ev::IndexLayout::single_node;
  else if (text == "packed-pages") layout = ev::IndexLayout::packed_pages;
  else return invalid("index layout must be node-files or packed-pages");
  return {};
}

bool number(std::string_view text,std::uint64_t& value){
  const auto r=std::from_chars(text.data(),text.data()+text.size(),value);
  return !text.empty()&&r.ec==std::errc{}&&r.ptr==text.data()+text.size();
}
#include "modular_session_application.inc"
s::Result durable_change(Application& app,std::string_view instance,std::string_view operation,const fs::path& file){
  if(auto r=app.runner->open();!r)return r;
  if(app.runner->progress().cycle_pending)return {s::Code::busy,"finish the pending controller cycle before changing an engine"};
  auto* engine=app.runtime->find(instance);
  if(!engine)return {s::Code::not_found,"active engine not found"};
  const auto* descriptor=app.catalog.find(engine->identity().type_id);
  if(!descriptor||descriptor->change_schemas.size()!=1)return {s::Code::unsupported,"engine has no operator change schema"};
  std::string bytes;
  if(!read_file(file,bytes,4U*1024U*1024U))return invalid("cannot read bounded change file");
  s::StepRequest request{std::string(operation),engine->state().revision,{descriptor->change_schemas.front(),bytes}};
  if(!s::valid_token(operation))return invalid("invalid operation ID");
  ev::StoredRecord existing;
  const auto found=app.store->latest("sdk.operator-intent",operation,existing);
  if(found){
    std::vector<std::string> fields;
    if(!s::unpack(existing.record.payload,fields,4)||fields.size()!=4||fields[0]!=s::encode_identity(engine->identity())||
      fields[2]!=s::encode_envelope(request.input)||fields[3]!="change"||!number(fields[1],request.expected_revision))
      return {s::Code::conflict,"operation ID was already bound to a different engine or change"};
  }else{
    if(found.code!=s::Code::not_found)return found;
    const auto payload=s::pack({s::encode_identity(engine->identity()),std::to_string(request.expected_revision),s::encode_envelope(request.input),"change"});
    ev::CommitBatch intent;
    intent.operation_id="intent-"+s::sha256(operation);
    intent.request_digest=s::sha256(payload);
    intent.guards.push_back({"sdk.operator-intent",std::string(operation),0,{}});
    intent.records.push_back({"sdk.operator-intent",std::string(operation),payload,{},false});
    ev::CommitReceipt receipt;
    if(auto r=app.store->commit(intent,receipt);!r.committed())return r;
  }
  return app.runtime->step(instance,request,true);
}
class MaximumMetric final:public s::IScoreStrategy {
  s::ScoreStrategyIdentity identity_;
public:
  MaximumMetric(std::string metric,std::string implementation):identity_{"anaiero.maximum-metric/1",std::move(implementation),
    {"anaiero.metric-score/1",std::move(metric)}}{}
  s::ScoreStrategyIdentity identity()const override{return identity_;}
  s::Result score(const std::vector<s::MetricComparison>& metrics,s::Score& output)const override{
    for(const auto& metric:metrics)if(metric.candidate.metric==identity_.configuration.payload){
      output={metric.candidate.value,true,"Maximum final "+metric.candidate.metric+" in "+metric.candidate.unit};
      return {};
    }
    return {s::Code::unsupported,"requested metric is absent from this engine"};
  }
};
void selection_json(const s::RecordedExperiment& value){
  std::cout<<"{\"experiment\":"<<json(value.specification.id)<<",\"completed\":"<<(value.completed?"true":"false")
    <<",\"source\":"<<json(value.source.source_uid)<<",\"source_revision\":"<<value.source_state.revision
    <<",\"source_state_sha256\":"<<json(s::state_digest(value.source_state))
    <<",\"chosen_candidate\":"<<json(value.result.chosen_candidate)<<",\"program_sha256\":"<<json(value.result.program_sha256)
    <<",\"baseline_score\":"<<value.result.baseline_score.value<<",\"candidates\":[";
  bool first=true;
  for(const auto& candidate:value.result.candidates){
    if(!first)std::cout<<',';
    first=false;
    std::cout<<"{\"id\":"<<json(candidate.id)<<",\"status\":"<<json(s::code_name(candidate.status.code))
      <<",\"score\":"<<candidate.score.value<<",\"eligible\":"<<(candidate.status&&candidate.score.eligible?"true":"false")<<'}';
  }
  std::cout<<"]}\n";
}
s::Result admin(Application& app,const fs::path& file){
  std::string text;
  if(!read_file(file,text,16384))return invalid("cannot read administration command");
  std::istringstream in(text);
  std::string family,verb,operation,extra;
  in>>family>>verb;
  auto end=[&]{return !(in>>extra);
  };
  if(family=="source"){
    if(verb!="inspect"){
      const auto opened=app.runner->open();
      if(!opened&&opened.code!=s::Code::not_found)return opened;
      if(opened&&app.runner->progress().cycle_pending)return {s::Code::busy,"finish the pending cycle before changing a source lifecycle"};
    }
    life::SourceRef ref;
    if(!(in>>ref.uid>>ref.epoch))return invalid("source requires UID and epoch");
    if(verb=="drain"){
      if(!(in>>operation)||!end())return invalid("source drain syntax");
      return app.lifecycle->begin_drain(ref,operation);
    }
    if(verb=="seal"){
      life::Cursor tip;
      if(!(in>>tip.sequence>>tip.payload_sha256>>operation)||!end())return invalid("source seal syntax");
      if(tip.payload_sha256=="-")tip.payload_sha256.clear();
      return app.lifecycle->seal(ref,tip,operation);
    }
    if(verb=="retire"){
      if(!(in>>operation)||!end())return invalid("source retire syntax");
      life::Source source;
      if(auto r=app.lifecycle->source(ref,source);!r)return r;
      if(auto r=app.lifecycle->retire(ref,operation);!r)return r;
      return detach_identity(app,source.identity);
    }
    if(verb=="inspect"){
      if(!end())return invalid("source inspect syntax");
      life::Source source;
      if(auto r=app.lifecycle->source(ref,source);!r)return r;
      std::cout<<"{\"source\":"<<json(ref.uid)<<",\"epoch\":"<<ref.epoch<<",\"phase\":"<<json(life::phase_name(source.phase))
        <<",\"sequence\":"<<source.last_sequence<<",\"payload_sha256\":"<<json(source.last_payload_sha256)<<"}\n";
        return {};
    }
  }else if(family=="peer"&&verb=="expire"){
    if(!(in>>operation)||!end())return invalid("peer expire syntax");
    return app.lifecycle->expire_peer_leases(now(),operation);
  }else if(family=="peer"&&verb=="upsert"){
    life::Peer peer;
    std::uint64_t expected=0;
    std::string phase;
    if(!(in>>peer.principal>>expected>>peer.generation>>peer.certificate_sha256>>peer.expires_unix_s>>phase>>operation)||!end())return invalid("peer upsert syntax");
    if(phase=="active")peer.phase=life::PeerPhase::active;
    else if(phase=="suspended")peer.phase=life::PeerPhase::suspended;
    else if(phase=="revoked")peer.phase=life::PeerPhase::revoked;
    else return invalid("unknown peer phase");
    return app.lifecycle->upsert_peer(peer,expected,now(),operation);
  }else if(family=="subscription"){
    if(verb=="open"){
      life::Subscription value;
      if(!(in>>value.id>>value.principal>>value.scope.uid>>value.scope.epoch>>value.generation>>value.lease_until_unix_s>>operation)||!end())return invalid("subscription open syntax");
      life::Subscription previous;
      const auto found=app.lifecycle->subscription(value.id,previous);
      if(found)value.acknowledged=previous.acknowledged;
      else if(found.code!=s::Code::not_found)return found;
      return app.lifecycle->open_subscription(value,now(),operation);
    }
    if(verb=="offer"||verb=="ack"){
      std::string id;
      std::uint64_t generation=0;
      life::Cursor cursor;
      if(!(in>>id>>generation>>cursor.sequence>>cursor.payload_sha256>>operation)||!end())return invalid("subscription cursor syntax");
      if(cursor.payload_sha256=="-")cursor.payload_sha256.clear();
      return verb=="offer"?app.lifecycle->offer(id,generation,cursor,now(),operation):app.lifecycle->acknowledge(id,generation,cursor,now(),operation);
    }
    if(verb=="close"){
      std::string id;
      std::uint64_t generation=0;
      if(!(in>>id>>generation>>operation)||!end())return invalid("subscription close syntax");
      return app.lifecycle->close_subscription(id,generation,operation);
    }
    if(verb=="expire"){
      if(!(in>>operation)||!end())return invalid("subscription expire syntax");
      return app.lifecycle->expire_subscriptions(now(),operation);
    }
  }else if(family=="bootstrap"&&verb=="accept"){
    life::Bootstrap value;
    std::string snapshot_file;
    if(!(in>>value.subscription_id>>value.principal>>value.scope.uid>>value.scope.epoch>>value.generation
       >>value.lease_until_unix_s>>value.tip.sequence>>value.tip.payload_sha256>>std::quoted(snapshot_file)>>operation)||!end())
      return invalid("bootstrap accept syntax");
    if(value.tip.payload_sha256=="-")value.tip.payload_sha256.clear();
    if(!read_file(snapshot_file,value.snapshot))return invalid("cannot read bounded bootstrap snapshot");
    value.snapshot_sha256=s::sha256(value.snapshot);
    return app.lifecycle->accept_bootstrap(value,now(),operation);
  }else if(family=="pin"){
    if(verb=="put"){
      life::Pin pin;
      std::uint64_t expected=0;
      unsigned kind=0;
      std::size_t count=0;
      if(!(in>>pin.id>>pin.owner>>expected>>pin.generation>>kind>>pin.expires_unix_s>>count)||kind>6||count>64)return invalid("pin put syntax");
      pin.kind=static_cast<life::PinKind>(kind);
      for(std::size_t i=0;i<count;++i){std::string digest;
      if(!(in>>digest))return invalid("missing pin digest");
      pin.objects.push_back(std::move(digest));
      }
      if(!(in>>operation)||!end())return invalid("pin operation missing");
      return app.lifecycle->put_pin(pin,expected,now(),operation);
    }
    if(verb=="release"){
      std::string id;
      std::uint64_t generation=0;
      if(!(in>>id>>generation>>operation)||!end())return invalid("pin release syntax");
      return app.lifecycle->release_pin(id,generation,operation);
    }
  }else if(family=="maintenance"){
    if(verb=="review"){
      life::MaintenanceRequest request;
      std::string disposition;
      std::size_t count=0;
      if(!(in>>request.id>>request.source.uid>>request.source.epoch>>request.expire_through>>disposition>>std::quoted(request.archive_directory)>>count)||count>64)
        return invalid("maintenance review syntax");
      if(disposition=="expire")request.disposition=life::HistoryAvailability::expired;
      else if(disposition=="archive")request.disposition=life::HistoryAvailability::archived;
      else return invalid("maintenance requires archive or explicit expiry");
      if(request.disposition==life::HistoryAvailability::archived){
        if(!app.archive)return invalid("archive maintenance requires the configured archive directory argument");
        const auto archive_native_path=[](std::string_view encoded_path){
          return fs::path(std::u8string(reinterpret_cast<const char8_t*>(encoded_path.data()),encoded_path.size()));
        };
        std::error_code path_error;
        if(!fs::equivalent(archive_native_path(request.archive_directory),archive_native_path(app.archive->identifier()),path_error)||path_error)
          return invalid("maintenance archive differs from the configured archive directory");
        request.archive_directory=app.archive->identifier();
      }
      for(std::size_t i=0;i<count;++i){std::string digest;
      if(!(in>>digest))return invalid("missing maintenance object");
      request.objects.push_back(std::move(digest));
      }
      if(!(in>>operation)||!end())return invalid("maintenance operation missing");
      request.now_unix_s=now();
      life::MaintenancePlan plan;
      auto result=app.lifecycle->review_maintenance(request,operation,plan);
      if(result)std::cout<<"{\"maintenance\":"<<json(plan.request.id)<<",\"expected_bytes\":"<<plan.expected_bytes<<",\"new_floor\":"<<plan.new_floor<<"}\n";
      return result;
    }
    if(verb=="activate"||verb=="execute"){
      std::string id;
      if(!(in>>id>>operation)||!end())return invalid("maintenance action syntax");
      return verb=="activate"?app.lifecycle->activate_maintenance(id,now(),operation):app.lifecycle->execute_maintenance(id,operation);
    }
  }else if(family=="history"&&verb=="status"){
    life::SourceRef source;
    std::uint64_t after=0;
    if(!(in>>source.uid>>source.epoch>>after)||!end())return invalid("history status syntax");
    return app.lifecycle->replay_status(source,after);
  }else if(family=="restore"&&verb=="reauthorize"){
    std::string policy_file;
    if(!(in>>std::quoted(policy_file)>>operation)||!end())return invalid("restore reauthorize syntax");
    std::string declaration;
    if(!read_file(fs::path(policy_file),declaration,16384)||declaration.empty())return invalid("a current operator policy declaration is required");
    if(auto r=app.lifecycle->reauthorize_restore(s::sha256(declaration),operation);!r)return r;
    std::vector<s::EngineIdentity> cleanup;
    if(auto r=app.lifecycle->reauthorization_sources(operation,cleanup);!r)return r;
    for(const auto& identity:cleanup){
      const auto detached=detach_identity(app,identity);
      if(!detached&&detached.code!=s::Code::not_found)return detached;
    }
    return {};
  }
  return invalid("unknown administration command; see the modular session guide");
}
s::Bytes encode_compaction_plan(const ev::CompactionPlan& p){
  const bool explicit_layout = p.options.target_index_layout != ev::IndexLayout::automatic;
  std::vector<std::string> fields{explicit_layout ? "anaiero.compaction-plan/2" : "anaiero.compaction-plan/1",p.options.operation_id,std::to_string(p.options.expected_generation),p.options.expected_root,
    std::to_string(p.options.working_budget_bytes),p.options.expire_operation_receipts?"1":"0",p.plan_digest,p.source_root,p.source_base,
    std::to_string(p.generation),std::to_string(p.next_operation_epoch),std::to_string(p.accounted_bytes_before),std::to_string(p.required_working_bytes),
    std::to_string(p.retained_records),std::to_string(p.retained_blobs),std::to_string(p.retained_operations),std::to_string(p.summarized_record_versions),
    std::to_string(p.expired_observations),std::to_string(p.canonical_transactions_removed)};
  if (explicit_layout) fields.emplace_back(target_layout_label(p.options.target_index_layout));
  return s::pack(fields);
}
s::Result decode_compaction_plan(std::string_view bytes,ev::CompactionPlan& p){
  std::vector<std::string> f;
  if (!s::unpack(bytes, f, 20, 16384) || f.empty()) return invalid("invalid compaction plan artifact");
  const bool legacy = f[0] == "anaiero.compaction-plan/1" && f.size() == 19;
  const bool explicit_layout = f[0] == "anaiero.compaction-plan/2" && f.size() == 20;
  if ((!legacy && !explicit_layout) || (f[5]!="0"&&f[5]!="1")||
     !s::valid_digest(f[6])||!s::valid_digest(f[7])||(!f[8].empty()&&!s::valid_digest(f[8])))return invalid("invalid compaction plan artifact");
  p.options.target_index_layout = ev::IndexLayout::automatic;
  if (explicit_layout) {
    if (auto result = parse_index_layout(f.back(), p.options.target_index_layout); !result) return result;
  }
  p.options.operation_id=f[1];p.options.expected_root=f[3];p.options.expire_operation_receipts=f[5]=="1";
  p.plan_digest=f[6];p.source_root=f[7];p.source_base=f[8];
  const std::pair<std::size_t,std::uint64_t*> numbers[]={{2,&p.options.expected_generation},{4,&p.options.working_budget_bytes},
    {9,&p.generation},{10,&p.next_operation_epoch},{11,&p.accounted_bytes_before},{12,&p.required_working_bytes},
    {13,&p.retained_records},{14,&p.retained_blobs},{15,&p.retained_operations},{16,&p.summarized_record_versions},
    {17,&p.expired_observations},{18,&p.canonical_transactions_removed}};
  for(const auto& [index,value]:numbers)if(!number(f[index],*value)||std::to_string(*value)!=f[index])return invalid("noncanonical compaction plan number");
  return {};
}
s::Result compaction_arguments(int argc, char** argv, ev::CompactionOptions& options) {
  bool budget_supplied = false;
  for (int i = 5; i < argc; ++i) {
    const std::string_view argument = argv[i];
    if (argument == "--index-layout") {
      if (options.target_index_layout != ev::IndexLayout::automatic || i + 1 >= argc) {
        return invalid("--index-layout requires one explicit target layout");
      }
      if (auto result = parse_index_layout(argv[++i], options.target_index_layout); !result) return result;
    } else if (argument == "expire-receipts") {
      if (options.expire_operation_receipts) return invalid("duplicate expire-receipts option");
      options.expire_operation_receipts = true;
    } else {
      if (budget_supplied || !number(argument, options.working_budget_bytes) || options.working_budget_bytes < 65536) {
        return invalid("invalid compaction working byte budget or option");
      }
      budget_supplied = true;
    }
  }
  return {};
}

s::Result open_maintenance_store(std::string_view directory,std::unique_ptr<ev::ICommitStore>& store){
  if(!canonical_root_exists(fs::path(directory)))return {s::Code::not_found,"canonical store ROOT does not exist"};
  ev::StoreOptions options;std::string identity;
  for(unsigned attempt=0;attempt<4;++attempt){
    store.reset();
    if(!canonical_root_exists(fs::path(directory)))return {s::Code::conflict,"canonical root disappeared during maintenance admission"};
    if(auto r=ev::open_native_store(directory,options,store);!r)return r;
    if(!identity.empty()&&identity!=store->info().store_id)return {s::Code::conflict,"store identity changed during maintenance admission"};
    identity=store->info().store_id;
    std::uint64_t persisted=0;bool exists=false;if(auto r=stored_budget(*store,persisted,exists);!r)return r;
    if(!exists||persisted==options.maximum_bytes)return {};
    options.maximum_bytes=persisted;
  }
  return {s::Code::busy,"store capacity policy changed repeatedly"};
}
s::Result compaction_receipt_admission(ev::ICommitStore& store,bool expire){
  if(!expire)return {};
  ev::StoredRecord session;const auto found=store.latest("session-definition","main",session);
  if(found)return {s::Code::unsupported,"receipt epoch rotation requires an epoch-aware SDK client; this modular session retains its original operation namespace"};
  if(found.code!=s::Code::not_found)return found;
  ev::StoredRecord mesh;const auto runtime=store.latest("operator.runtime","canonical-mesh",mesh);
  if(runtime)return {s::Code::unsupported,"built-in canonical mesh retains its original operation namespace; receipt epoch rotation requires an epoch-aware SDK client"};
  if(runtime.code!=s::Code::not_found)return runtime;
  std::vector<ev::StoredRecord> grants;bool more=false;
  if(auto r=store.scan_latest("mesh.grant",{},{},1,grants,more);!r)return r;
  if(!grants.empty())return {s::Code::unsupported,"retained mesh grants require the original operation namespace"};
  return {};
}
void compaction_report(const ev::CompactionReport& r, const ev::StoreInfo& info){
  std::cout<<"{\"compaction\":\"canonical-base\",\"plan_sha256\":"<<json(r.plan_digest)<<",\"canonical_base_sha256\":"<<json(r.canonical_base_digest)
    <<",\"index_layout\":"<<json(index_layout_label(info.index_backend))
    <<",\"generation\":"<<r.generation<<",\"operation_epoch\":"<<r.operation_epoch<<",\"accounted_bytes_before\":"<<r.accounted_bytes_before
    <<",\"accounted_bytes_after\":"<<r.accounted_bytes_after<<",\"removed_transaction_bytes\":"<<r.removed_transaction_bytes
    <<",\"removed_index_bytes\":"<<r.removed_index_bytes<<",\"summarized_record_versions\":"<<r.summarized_record_versions
    <<",\"expired_observations\":"<<r.expired_observations<<",\"activated\":"<<(r.activated?"true":"false")
    <<",\"cleanup_complete\":"<<(r.cleanup_complete?"true":"false")<<",\"already_complete\":"<<(r.already_complete?"true":"false")<<"}\n";
}
void help(){
  std::cout<<"Anaiero modular sessions\n"
    "  catalog\n  init CONFIG STORE [MAXIMUM_BYTES]\n  configure CONFIG STORE OPERATION\n  run STORE THROUGH_CYCLE\n  status STORE\n"
    "  change STORE INSTANCE OPERATION CHANGE_FILE\n  pause|resume|cancel STORE INSTANCE OPERATION\n"
    "  migration-review STORE INSTANCE OPERATION TARGET_SCHEMA PLAN_FILE\n"
    "  migration-activate STORE INSTANCE PLAN_FILE\n  migration-status STORE OPERATION\n"
    "  trial STORE INSTANCE TICKS [CHANGE_FILE]\n  export-plan STORE INSTANCE IMAGE RECEIPTS\n"
    "  select STORE INSTANCE EXPERIMENT_ID TICKS METRIC CHANGE_FILE [CHANGE_FILE...]\n"
    "  selection|resume-experiment STORE EXPERIMENT_ID\n  apply-selected|outcome|release-experiment STORE EXPERIMENT_ID OPERATION\n"
    "  audit|repair-index STORE [WORKING_BUDGET_BYTES]\n  capacity STORE MAXIMUM_BYTES OPERATION\n  admin STORE COMMAND_FILE [ARCHIVE_DIRECTORY]\n"
    "  compact-review STORE OPERATION PLAN_FILE [WORKING_BUDGET_BYTES] [--index-layout node-files|packed-pages] [expire-receipts]\n  compact STORE PLAN_FILE\n  compact-resume STORE [RECOVERY_WORKING_BUDGET_BYTES]\n"
    "  migrate-evidence LEGACY_STORE NEW_STORE\n  restore SOURCE_STORE NEW_STORE OPERATION\n"
    "  module-packages|host-packages STORE [AFTER_PACKAGE]\n  export-artifact STORE SHA256 OUTPUT\n"
    "  export-state STORE INSTANCE FILE\n"
    "Run uses an absolute committed cycle target. Restart and repeat the same target to finish interrupted work.\n"
    "Changes and controls require stable caller operation IDs. Plan and model artifacts are retained at init.\n";
}
s::Result execute(int argc,char** argv){
  if(argc<2||std::string_view(argv[1])=="help"){help();
  return {};
  }
  const std::string command=argv[1];
  if(command=="catalog"){
    s::EngineModuleCatalog catalog;
    if(auto r=e::register_builtin_engines(catalog);!r)return r;
    for(const auto& d:catalog.describe())std::cout<<"{\"type\":"<<json(d.type_id)<<",\"name\":"<<json(d.display_name)
      <<",\"module_sha256\":"<<json(d.module_sha256)<<",\"finite\":"<<(d.finite?"true":"false")
      <<",\"isolated_trials\":"<<(d.isolated_trials?"true":"false")<<",\"changes\":"<<(d.change_preparation?"true":"false")<<"}\n";
    return {};
  }
  if(argc<3)return invalid("store path is required");
  if(command=="compact-review"||command=="compact"||command=="compact-resume"){
    if((command=="compact-review"&&(argc<5||argc>9))||(command=="compact"&&argc!=4)||(command=="compact-resume"&&(argc!=3&&argc!=4)))
      return invalid("invalid canonical compaction command; see help");
    if(command=="compact-resume"){
      // The retained capacity policy is indexed. Recovery must first establish
      // coverage from the canonical base; consulting latest() before repair
      // would block the after-ROOT/before-index interruption it must recover.
      if(!canonical_root_exists(argv[2]))return {s::Code::not_found,"canonical store ROOT does not exist"};
      ev::StoreOptions recovery;
      if(argc==4&&(!number(argv[3],recovery.maximum_bytes)||recovery.maximum_bytes<65536))return invalid("invalid compaction recovery working budget");
      std::unique_ptr<ev::ICommitStore> store;if(auto r=ev::open_native_store(argv[2],recovery,store);!r)return r;
      ev::CompactionReport report;const auto result=ev::resume_native_compaction(*store,report);
      compaction_report(report,store->info());return result;
    }
    std::unique_ptr<ev::ICommitStore> store;if(auto r=open_maintenance_store(argv[2],store);!r)return r;
    ev::CompactionPlan plan;
    if(command=="compact-review"){
      ev::CompactionOptions options;options.operation_id=argv[3];options.expected_generation=store->generation();options.expected_root=store->info().root_digest;
      if (auto result = compaction_arguments(argc, argv, options); !result) return result;
      if(auto r=compaction_receipt_admission(*store,options.expire_operation_receipts);!r)return r;
      if(auto r=ev::preview_native_compaction(*store,options,plan);!r)return r;
      if(auto r=e::write_new_artifact(argv[4],encode_compaction_plan(plan));!r)return r;
      const auto source_backend = store->info().index_backend;
      const auto target = options.target_index_layout == ev::IndexLayout::automatic
                        ? index_layout_label(source_backend) : target_layout_label(options.target_index_layout);
      std::cout<<"{\"compaction\":\"reviewed\",\"source_index_layout\":"<<json(index_layout_label(source_backend))
        <<",\"target_index_layout\":"<<json(target)<<",\"plan_sha256\":"<<json(plan.plan_digest)<<",\"source_root\":"<<json(plan.source_root)
        <<",\"source_generation\":"<<plan.generation<<",\"required_working_bytes\":"<<plan.required_working_bytes
        <<",\"retained_records\":"<<plan.retained_records<<",\"retained_blob_identities\":"<<plan.retained_blobs
        <<",\"retained_operations\":"<<plan.retained_operations<<",\"summarized_record_versions\":"<<plan.summarized_record_versions
        <<",\"expired_observations\":"<<plan.expired_observations<<",\"transactions_removed\":"<<plan.canonical_transactions_removed
        <<",\"next_operation_epoch\":"<<plan.next_operation_epoch<<",\"plan_file\":"<<json(argv[4])<<"}\n";
      return {};
    }
    ev::CompactionReport report;s::Result result;
    if(command=="compact"){
      std::string bytes;if(!read_file(fs::path(argv[3]),bytes,16384))return invalid("cannot read bounded compaction plan artifact");
      if(auto r=decode_compaction_plan(bytes,plan);!r)return r;
      if(auto r=compaction_receipt_admission(*store,plan.options.expire_operation_receipts);!r)return r;
      result=ev::execute_native_compaction(*store,plan,report);
    }
    compaction_report(report,store->info());return result;
  }
  if(((command=="module-packages"||command=="host-packages")&&(argc==3||argc==4))||(command=="export-artifact"&&argc==5)){
    if(!canonical_root_exists(argv[2]))return {s::Code::not_found,"canonical store ROOT does not exist"};
    std::unique_ptr<ev::ICommitStore> store;
    if(auto r=ev::open_native_store(argv[2],{},store);!r)return r;
    if(command=="export-artifact"){
      if(!s::valid_digest(argv[3]))return invalid("artifact SHA-256 is invalid");
      std::string bytes;if(auto r=store->get_blob(argv[3],bytes);!r)return r;
      return e::write_new_artifact(argv[4],bytes);
    }
    std::vector<ev::StoredRecord> records;bool more=false;
    const bool host_package=command=="host-packages";
    if(auto r=store->scan_latest(host_package?"sdk.host-installation":"sdk.module-installation",{},argc==4?argv[3]:"",32,records,more);!r)return r;
    std::cout<<"{\"packages\":[";bool first=true;
    for(const auto& record:records){
      if(record.record.tombstone)continue;
      s::Digest module=record.record.key,artifact=record.record.payload;
      if(host_package){
        std::vector<std::string> fields;
        if(!s::unpack(record.record.payload,fields,3,1024)||fields.size()!=3||fields[0]!="native-host-package/1"||
           record.record.key!=s::sha256(s::pack({fields[1],fields[2]})))return {s::Code::corrupt,"retained host package identity is invalid"};
        module=fields[1];artifact=fields[2];
      }
      if(!s::valid_digest(module)||!s::valid_digest(artifact))return {s::Code::corrupt,"retained module package identity is invalid"};
      if(!first)std::cout<<',';
      first=false;
      std::cout<<"{\"kind\":"<<json(host_package?"host":"session")<<",\"package_id\":"<<json(record.record.key)
        <<",\"module_sha256\":"<<json(module)<<",\"artifact_sha256\":"<<json(artifact)<<'}';
    }
    std::cout<<"],\"more\":"<<(more?"true":"false")<<",\"after\":"<<json(records.empty()?"":records.back().record.key)<<"}\n";
    return {};
  }
  if(command=="capacity"&&argc==5){
    std::uint64_t maximum=0;
    if(!number(argv[3],maximum)||maximum<65536||!s::valid_token(argv[4]))return invalid("capacity requires a byte budget and stable operation ID");
    if(!canonical_root_exists(argv[2]))return {s::Code::not_found,"canonical store ROOT does not exist"};
    ev::StoreOptions options;options.maximum_bytes=maximum;
    std::unique_ptr<ev::ICommitStore> store;
    if(auto r=ev::open_native_store(argv[2],options,store);!r)return r;
    ev::StoredRecord prior;
    const auto found=store->latest("operator.policy","capacity",prior);
    if(!found&&found.code!=s::Code::not_found)return found;
    const auto payload=s::pack({"store-budget/1",std::to_string(maximum)});
    ev::CommitBatch batch;batch.operation_id=argv[4];batch.request_digest=s::sha256(payload);
    batch.commit_class=ev::CommitClass::maintenance;
    batch.guards.push_back({"operator.policy","capacity",prior.revision,prior.digest});
    batch.records.push_back({"operator.policy","capacity",payload,{},false});
    ev::CommitReceipt receipt;if(auto r=store->commit(batch,receipt);!r)return r;
    std::cout<<"{\"capacity\":\"committed\",\"maximum_bytes\":"<<maximum<<",\"generation\":"<<receipt.generation<<"}\n";
    return {};
  }
  if((command=="audit"||command=="repair-index")&&(argc==3||argc==4)){
    if(!canonical_root_exists(argv[2]))return {s::Code::not_found,"canonical store ROOT does not exist"};
    ev::StoreOptions options;
    if(argc==4&&(!number(argv[3],options.maximum_bytes)||options.maximum_bytes<65536))return invalid("invalid inspection working budget");
    std::unique_ptr<ev::ICommitStore> store;
    if(auto r=ev::open_native_store(argv[2],options,store);!r)return r;
    if(command=="repair-index")if(auto r=store->repair_index();!r)return r;
    ev::AuditReport report;
    if(auto r=store->audit(report);!r)return r;
    std::cout<<"{\"audit\":\"verified\",\"generations\":"<<report.generations<<",\"records\":"<<report.records
      <<",\"blobs\":"<<report.blobs<<",\"expired_blobs\":"<<report.expired_blobs
      <<",\"summarized_generations\":"<<report.summarized_generations<<",\"summarized_record_versions\":"<<report.summarized_record_versions
      <<",\"expired_observations\":"<<report.expired_observations<<",\"base_pages\":"<<report.base_pages<<",\"canonical_base_sha256\":"<<json(report.canonical_base_digest)
      <<",\"canonical_records_read\":"<<report.canonical_records_read<<",\"index_nodes_read\":"<<report.index_nodes_read<<"}\n";
    return {};
  }
  if(command=="migrate-evidence"){
    if(argc!=4)return invalid("migrate-evidence requires legacy source and new destination");
    ev::MigrationReport report;
    if(auto r=ev::migrate_legacy_evidence(argv[2],argv[3],{},report);!r)return r;
    std::cout<<"{\"migration\":\"evidence-only\",\"source_fingerprint\":"<<json(report.source_fingerprint)
      <<",\"destination_root\":"<<json(report.destination_root)<<",\"objects\":"<<report.objects<<",\"events\":"<<report.events
      <<",\"source_unchanged\":"<<(report.source_unchanged?"true":"false")<<",\"activated\":"<<(report.activated?"true":"false")
      <<",\"backup_directory\":"<<json(report.backup_directory)<<",\"retained_backup_bytes\":"<<report.retained_backup_bytes<<"}\n";
    return {};
  }
  if(command=="restore"){
    if(argc!=5)return invalid("restore requires source, new destination and stable operation ID");
    life::RestoreReport report;
    if(auto r=life::restore_quarantined_store(argv[2],argv[3],argv[4],report);!r)return r;
    std::cout<<"{\"restore\":\"quarantined\",\"source_generation\":"<<report.source_generation
      <<",\"source_root\":"<<json(report.source_root)<<",\"incarnation\":"<<json(report.incarnation)
      <<",\"copied_files\":"<<report.copied_files<<",\"copied_bytes\":"<<report.copied_bytes
      <<",\"published\":"<<(report.published?"true":"false")<<"}\n";
    return {};
  }
  Application app;
  if(command=="configure"){
    if(argc!=5)return invalid("configure requires configuration, store and operation ID");
    if(auto r=app.open(argv[3],false);!r)return r;
    s::SessionDefinition definition;
    if(auto r=e::read_session_definition(argv[2],app.catalog,definition);!r)return r;
    for(const auto& entry:definition.engines)if(!app.runtime->find(entry.identity.instance_id)){
      std::unique_ptr<s::IEngine> engine;
      if(auto r=app.catalog.create(entry.identity,entry.configuration,nullptr,engine);!r)return r;
    }
    if(auto r=app.retain_executable(executable(argv[0]));!r)return r;
    if(auto r=app.runner->reconfigure(definition,argv[4]);!r)return r;
    if(auto r=app.ensure_engines();!r)return r;
    return app.status();
  }
  if(command=="init"){
    if(argc<4||argc>5)return invalid("init requires configuration and store");
    std::uint64_t maximum=0;
    if(argc==5&&(!number(argv[4],maximum)||maximum<65536))return invalid("invalid store byte quota");
    if(auto r=app.open(argv[3],true,{},maximum);!r)return r;
    s::SessionDefinition definition;
    if(auto r=e::read_session_definition(argv[2],app.catalog,definition);!r)return r;
    // Validate every module before writing an initialized session intent.
    for(const auto& entry:definition.engines){std::unique_ptr<s::IEngine> engine;
      if(auto r=app.catalog.create(entry.identity,entry.configuration,nullptr,engine);!r)return r;
      }
    if(auto r=app.runner->initialize(definition);!r)return r;
    if(auto r=app.retain_executable(executable(argv[0]));!r)return r;
    if(auto r=app.ensure_engines();!r)return r;
    return app.status();
  }
  const std::string_view archive_path=command=="admin"&&argc==5?argv[4]:"";
  if(auto r=app.open(argv[2],false,archive_path);!r)return r;
  if(command=="status"&&argc==3){return app.status();
  }
  if(command=="migration-status"&&argc==4) {
    ev::StoredMigrationReceipt audit;
    if(auto r=ev::read_migration_receipt(*app.store,argv[3],audit);!r)return r;
    const auto& plan=audit.receipt.plan;
    std::cout<<"{\"migration\":\"committed\",\"instance\":"<<json(plan.identity.instance_id)
      <<",\"operation\":"<<json(plan.request.operation_id)
      <<",\"source_schema\":"<<json(plan.request.source_schema)
      <<",\"target_schema\":"<<json(plan.request.target_schema)
      <<",\"source_checkpoint_sha256\":"<<json(plan.source_checkpoint_sha256)
      <<",\"target_checkpoint_sha256\":"<<json(plan.target_checkpoint_sha256)
      <<",\"generation\":"<<audit.generation<<"}\n";
    return {};
  }
  if(command=="migration-review"||command=="migration-activate") {
    if((command=="migration-review"&&argc!=7)||(command=="migration-activate"&&argc!=5))
      return invalid("invalid engine migration arguments; see help");
    if(auto r=app.runner->open();!r)return r;
    if(app.runner->progress().cycle_pending)
      return {s::Code::busy,"finish the durable cycle before migration"};
    if(auto r=app.ensure_engines();!r)return r;
    if(command=="migration-review") {
      auto* engine=app.runtime->find(argv[3]);
      if(!engine)return {s::Code::not_found,"migration engine not found"};
      auto* migrator=engine->migrations();
      if(!migrator)return {s::Code::unsupported,"engine has no migration capability"};
      s::MigrationRequest request{argv[4],engine->state().revision,engine->state().checkpoint.schema,
        argv[5],std::string(migrator->implementation_sha256()),{"anaiero.migration-parameters/1",{}}};
      s::MigrationPlan plan;
      if(auto r=app.runtime->review_migration(argv[3],request,plan);!r)return r;
      const auto bytes=s::encode_migration_plan(plan);
      if(bytes.empty())return {s::Code::corrupt,"review produced an invalid migration plan"};
      if(auto r=e::write_new_artifact(argv[6],bytes);!r)return r;
      std::cout<<"{\"migration\":\"reviewed\",\"instance\":"<<json(argv[3])
        <<",\"source_schema\":"<<json(request.source_schema)
        <<",\"target_schema\":"<<json(request.target_schema)
        <<",\"source_checkpoint_sha256\":"<<json(plan.source_checkpoint_sha256)
        <<",\"target_checkpoint_sha256\":"<<json(plan.target_checkpoint_sha256)
        <<",\"plan_sha256\":"<<json(s::sha256(bytes))
        <<",\"plan_file\":"<<json(argv[6])<<"}\n";
      return {};
    }
    std::string bytes;
    if(!read_file(fs::path(argv[4]),bytes,4U*1024U*1024U+16384U))
      return invalid("cannot read bounded migration plan");
    s::MigrationPlan plan;
    if(auto r=s::decode_migration_plan(bytes,plan);!r)return r;
    const auto applied=app.runtime->apply_migration(argv[3],plan);
    if(applied.committed())
      std::cout<<"{\"migration\":\"committed\",\"instance\":"<<json(argv[3])
        <<",\"operation\":"<<json(plan.request.operation_id)
        <<",\"target_checkpoint_sha256\":"<<json(plan.target_checkpoint_sha256)
        <<",\"generation\":"<<app.store->info().generation<<"}\n";
    return applied;
  }
  if(command=="run"&&argc==4){std::uint64_t through=0;
  if(!number(argv[3],through))return invalid("invalid absolute cycle target");
    if(auto r=app.run(through);!r)return r;
    return app.status();
    }
  if(command=="admin"&&(argc==4||argc==5))return admin(app,argv[3]);
  if(command=="change"&&argc==6)return durable_change(app,argv[3],argv[4],argv[5]);
  if(command=="select"&&argc>=8&&argc<=15){
    if(auto r=app.runner->open();!r)return r;
    if(app.runner->progress().cycle_pending)return {s::Code::busy,"finish the pending cycle before freezing an experiment"};
    auto* engine=app.runtime->find(argv[3]);
    if(!engine)return {s::Code::not_found,"engine not found"};
    const auto* descriptor=app.catalog.find(engine->identity().type_id);
    if(!descriptor||!descriptor->isolated_trials||descriptor->change_schemas.size()!=1)
      return {s::Code::unsupported,"engine has no registered isolated change trials"};
    std::uint64_t ticks=0;
    if(!number(argv[5],ticks)||ticks==0||ticks>128||!s::valid_token(argv[6]))return invalid("selection requires 1..128 ticks and a metric name");
    const auto& schedules=app.runner->definition().engines;
    const auto scheduled=std::find_if(schedules.begin(),schedules.end(),[&](const auto& item){return item.identity==engine->identity();});
    if(scheduled==schedules.end())return {s::Code::not_found,"engine input schedule not found"};
    s::ExperimentSpecification specification;
    specification.id=argv[4];specification.inputs.assign(static_cast<std::size_t>(ticks),scheduled->input);
    for(int i=7;i<argc;++i){
      std::string bytes;if(!read_file(argv[i],bytes,4U*1024U*1024U))return invalid("cannot read bounded candidate change");
      specification.candidates.push_back({"candidate-"+std::to_string(i-6),{descriptor->change_schemas.front(),std::move(bytes)}});
    }
    MaximumMetric strategy(argv[6],descriptor->module_sha256);
    s::RecordedExperiment result;
    if(auto r=s::ExperimentHistoryService(app.catalog,*app.store).select(*engine,specification,strategy,result);!r)return r;
    selection_json(result);return {};
  }
  if(command=="selection"&&argc==4){
    s::RecordedExperiment result;
    if(auto r=s::ExperimentHistoryService(app.catalog,*app.store).load(argv[3],result);!r)return r;
    selection_json(result);return {};
  }
  if(command=="resume-experiment"&&argc==4){
    s::ExperimentHistoryService history(app.catalog,*app.store);
    s::RecordedExperiment frozen;
    if(auto r=history.load(argv[3],frozen);!r)return r;
    const auto* descriptor=app.catalog.find(frozen.source.type_id);
    if(!descriptor||descriptor->module_sha256!=frozen.strategy.implementation_sha256||
       frozen.strategy.id!="anaiero.maximum-metric/1"||frozen.strategy.configuration.schema!="anaiero.metric-score/1")
      return {s::Code::conflict,"retained experiment needs its pinned scorer implementation"};
    std::unique_ptr<s::IEngine> source;
    if(auto r=app.catalog.create(frozen.source,frozen.source_state.configuration,&frozen.source_state,source);!r)return r;
    MaximumMetric strategy(frozen.strategy.configuration.payload,descriptor->module_sha256);
    s::RecordedExperiment result;
    if(auto r=history.select(*source,frozen.specification,strategy,result);!r)return r;
    selection_json(result);return {};
  }
  if((command=="apply-selected"||command=="outcome"||command=="release-experiment")&&argc==5){
    s::ExperimentHistoryService history(app.catalog,*app.store);
    if(command=="release-experiment")return history.release(argv[3],argv[4]);
    if(command=="apply-selected"){
      if(auto r=app.runner->open();!r)return r;
      if(app.runner->progress().cycle_pending)return {s::Code::busy,"finish the pending cycle before applying a selection"};
      const auto result=history.apply_selected(argv[3],argv[4],*app.runtime);
      if(result.code==s::Code::completed){std::cout<<"{\"application\":\"baseline-preserved\"}\n";return {};}
      return result;
    }
    s::RecordedExperiment frozen;
    if(auto r=history.load(argv[3],frozen);!r)return r;
    if(frozen.strategy.id!="anaiero.maximum-metric/1"||frozen.strategy.configuration.schema!="anaiero.metric-score/1")
      return {s::Code::unsupported,"operator scorer does not implement the frozen strategy"};
    const auto* descriptor=app.catalog.find(frozen.source.type_id);
    if(!descriptor||descriptor->module_sha256!=frozen.strategy.implementation_sha256)
      return {s::Code::conflict,"frozen scorer implementation does not match this executable"};
    MaximumMetric strategy(frozen.strategy.configuration.payload,descriptor->module_sha256);
    s::OutcomeReview result;
    if(auto r=history.review(argv[3],argv[4],strategy,result);!r)return r;
    std::cout<<"{\"outcome\":\"committed\",\"measured_score\":"<<result.measured_score.value
      <<",\"meets_selected_score\":"<<(result.meets_selected_score?"true":"false")
      <<",\"exact_selected_state\":"<<(result.exact_selected_state?"true":"false")
      <<",\"observed_receipt_tip\":"<<json(result.observed_receipt_tip)<<"}\n";
    return {};
  }
  if((command=="pause"||command=="resume"||command=="cancel")&&argc==5){
    if(auto r=app.runner->open();!r)return r;
    if(app.runner->progress().cycle_pending)return {s::Code::busy,"finish the pending cycle before lifecycle control"};
    return app.runtime->control(argv[3],argv[4],command=="pause"?s::Phase::paused:command=="resume"?s::Phase::running:s::Phase::cancelled);
  }
  if(command=="export-plan"&&argc==6){
    auto* engine=app.runtime->find(argv[3]);
    if(!engine)return {s::Code::not_found,"engine not found"};
    std::string image,receipts;
    if(auto r=e::plan_output(engine->state(),image,receipts);!r)return r;
    if(auto r=e::write_new_artifact(argv[4],image);!r)return r;
    if(auto r=e::write_new_artifact(argv[5],receipts);!r)return r;
    return {};
  }
  if(command=="export-state"&&argc==5){
    auto* engine=app.runtime->find(argv[3]);
    if(!engine)return {s::Code::not_found,"engine not found"};
    if(auto r=e::write_new_artifact(argv[4],s::encode_state(engine->state()));!r)return r;
    return {};
  }
  if(command=="trial"&&(argc==5||argc==6)){
    if(auto r=app.runner->open();!r)return r;
    auto* engine=app.runtime->find(argv[3]);
    if(!engine)return {s::Code::not_found,"engine not found"};
    std::uint64_t ticks=0;
    if(!number(argv[4],ticks)||ticks>128)return invalid("trial horizon must be 0..128");
    const s::EngineSchedule* scheduled=nullptr;
    for(const auto& value:app.runner->definition().engines)if(value.identity.instance_id==argv[3])scheduled=&value;
    if(!scheduled)return {s::Code::not_found,"scheduled input program not found"};
    s::TrialProgram program;
    program.id="operator-trial";
    program.inputs.assign(static_cast<std::size_t>(ticks),scheduled->input);
    if(argc==6){const auto* d=app.catalog.find(engine->identity().type_id);
    std::string bytes;
      if(!d||d->change_schemas.size()!=1||!read_file(argv[5],bytes,4U*1024U*1024U))return invalid("trial change file or capability is invalid");
      program.has_change=true;
      program.change={d->change_schemas.front(),std::move(bytes)};
      }
    s::TrialResult result;
    if(auto r=s::ExperimentService{}.evaluate(*engine,program,result);!r)return r;
    std::cout<<"{\"trial\":\"isolated\",\"source_state_sha256\":"<<json(s::state_digest(engine->state()))
      <<",\"final_state_sha256\":"<<json(s::state_digest(result.final_state))<<",\"tick\":"<<result.final_state.tick
      <<",\"receipt_tip\":"<<json(result.receipt_tip)<<"}\n";
      return {};
  }
  return invalid("unknown command or argument count; use help");
}
}
int main(int argc,char** argv){
  try {const auto result=execute(argc,argv);
  if(!result){std::cerr<<"{\"status\":"<<json(s::code_name(result.code))<<",\"message\":"<<json(result.message)<<"}\n";
      return result.code==s::Code::committed_degraded?3:2;
      }return 0;
  }catch(const std::exception& error){std::cerr<<"{\"status\":\"io_error\",\"message\":"<<json(error.what())<<"}\n";
  return 2;
  }
}

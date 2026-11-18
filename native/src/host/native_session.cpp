#include "anaiero/sdk/native_session.hpp"
#include "anaiero/sdk/engine_files.hpp"
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace anaiero::sdk::host {
namespace {
namespace fs=std::filesystem;
namespace ev=evidence;
namespace life=lifecycle;
fs::path utf8_path(std::string_view text) {
  return fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(text.data()),text.size()));
}
class LifecyclePolicy final:public ICommitPolicy {
  life::Service& service_;
public:
  explicit LifecyclePolicy(life::Service& value):service_(value){}
  Result enrich(ev::CommitBatch& batch,const PreparedTransition& transition) override {
    return service_.stage_commit(batch,transition);
  }
  Result scheduling_admission(const EngineIdentity& identity,bool& eligible) const override {
    life::Source source;eligible=false;
    if(auto result=service_.source({identity.source_uid,identity.epoch},source);!result)return result;
    if(source.identity!=identity)return {Code::conflict,"scheduled lifecycle identity differs"};
    eligible=source.phase==life::SourcePhase::active;return {};
  }
  Result restore_admission(const EngineIdentity& identity) const override {
    life::Snapshot snapshot;if(auto result=service_.snapshot(snapshot);!result)return result;
    if(snapshot.quarantined)return {Code::unauthorized,"restored store is quarantined"};
    life::Source source;if(auto result=service_.source({identity.source_uid,identity.epoch},source);!result)return result;
    if(source.identity!=identity)return {Code::conflict,"retained lifecycle engine identity differs"};
    return source.phase==life::SourcePhase::retired?Result{Code::expired,"source is retired"}:Result{};
  }
  Result recovery_sources(std::vector<EngineIdentity>& identities) const override {
    life::Snapshot snapshot;if(auto result=service_.snapshot(snapshot);!result)return result;
    if(snapshot.quarantined)return {Code::unauthorized,"restored store is quarantined"};
    identities.clear();
    for(const auto& source:snapshot.sources)
      if(source.phase!=life::SourcePhase::retired&&!source.state_sha256.empty())identities.push_back(source.identity);
    return {};
  }
};
bool root_exists(const fs::path& directory) {
  std::error_code error;return fs::is_regular_file(fs::symlink_status(directory/"ROOT",error))&&!error;
}
Result capacity(ev::ICommitStore& store,std::uint64_t& bytes,bool& exists) {
  ev::StoredRecord record;const auto result=store.latest("operator.policy","capacity",record);
  exists=false;if(result.code==Code::not_found)return {};if(!result)return result;
  std::vector<std::string> fields;
  if(record.record.tombstone||!unpack(record.record.payload,fields,2,1024)||fields.size()!=2||fields[0]!="store-budget/1")
    return {Code::corrupt,"invalid persisted capacity policy"};
  const auto parsed=std::from_chars(fields[1].data(),fields[1].data()+fields[1].size(),bytes);
  if(parsed.ec!=std::errc{}||parsed.ptr!=fields[1].data()+fields[1].size()||std::to_string(bytes)!=fields[1]||bytes<65536)
    return {Code::corrupt,"invalid persisted capacity value"};
  exists=true;return {};
}
std::uint64_t now() {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
    std::chrono::system_clock::now().time_since_epoch()).count());
}
}
struct NativeSession::Impl {
  EngineModuleCatalog catalog;
  std::unique_ptr<ev::ICommitStore> store;
  std::unique_ptr<life::Service> lifecycle;
  ProjectionService projection;
  std::unique_ptr<LifecyclePolicy> policy;
  std::unique_ptr<SessionService> runtime;
  std::unique_ptr<SessionRunner> runner;
};
NativeSession::NativeSession():impl_(std::make_unique<Impl>()){}
NativeSession::~NativeSession()=default;
Result NativeSession::open(std::string_view directory) {
  auto& p=*impl_;
  if(p.store)return {Code::conflict,"native session is already open"};
  const auto path=utf8_path(directory);
  if(!root_exists(path))return {Code::not_found,"canonical store ROOT does not exist; initialize the session first"};
  if(auto result=anaiero::engines::register_builtin_engines(p.catalog);!result)return result;
  if(auto result=p.catalog.freeze();!result)return result;
  ev::StoreOptions options;
  if(auto result=ev::open_native_store(directory,options,p.store);!result)return result;
  const auto identity=p.store->info().store_id;
  for(unsigned attempt=0;attempt<4;++attempt) {
    std::uint64_t maximum=0;bool exists=false;
    if(auto result=capacity(*p.store,maximum,exists);!result)return result;
    if(!exists||maximum==options.maximum_bytes)break;
    if(attempt==3)return {Code::busy,"capacity policy changed repeatedly during open"};
    p.store.reset();options.maximum_bytes=maximum;
    if(!root_exists(path))return {Code::conflict,"canonical root disappeared while reopening capacity"};
    if(auto result=ev::open_native_store(directory,options,p.store);!result)return result;
    if(p.store->info().store_id!=identity)return {Code::conflict,"store identity changed while reopening capacity"};
  }
  ev::StoredRecord definition;
  if(auto result=p.store->latest("session-definition","main",definition);!result)return result;
  if(definition.record.tombstone)return {Code::expired,"session definition is retired"};
  p.lifecycle=std::make_unique<life::Service>(*p.store);
  if(auto result=p.lifecycle->open();!result)return result;
  p.policy=std::make_unique<LifecyclePolicy>(*p.lifecycle);
  p.runtime=std::make_unique<SessionService>(p.catalog,*p.store,p.projection,p.policy.get());
  if(auto result=p.runtime->recover();!result)return result;
  p.runner=std::make_unique<SessionRunner>(*p.runtime,*p.store);
  return p.runner->open();
}
Result NativeSession::ensure_engines() {
  auto& p=*impl_;
  if(!p.runner)return {Code::busy,"native session is not open"};
  for(const auto& scheduled:p.runner->definition().engines) {
    const auto& id=scheduled.identity;life::Source source;
    const auto found=p.lifecycle->source({id.source_uid,id.epoch},source);
    if(found.code==Code::not_found) {
      if(auto result=p.lifecycle->register_engine(id,"local-operator","enroll-"+sha256(encode_identity(id)));!result)return result;
      if(auto result=p.lifecycle->source({id.source_uid,id.epoch},source);!result)return result;
    }else if(!found)return found;
    if(source.identity!=id)return {Code::conflict,"configured source has a different lifecycle identity"};
    if(source.phase==life::SourcePhase::registered)
      if(auto result=p.lifecycle->activate(source.id,"activate-"+sha256(encode_identity(id)));!result)return result;
    if(source.phase==life::SourcePhase::retired||source.phase==life::SourcePhase::sealed||source.phase==life::SourcePhase::draining)continue;
    if(!p.runtime->find(id.instance_id))
      if(auto result=p.runtime->add_engine(id,scheduled.configuration);!result)return result;
  }
  return {};
}
Result NativeSession::retain_host_artifact(std::string_view path) {
  auto& p=*impl_;
  if(!p.store)return {Code::busy,"native session is not open"};
  std::error_code error;const auto file=utf8_path(path);
  if(!fs::is_regular_file(fs::symlink_status(file,error))||error)return {Code::invalid,"host executable is not a regular file"};
  const auto size=fs::file_size(file,error);
  if(error||size>8U*1024U*1024U)return {Code::capacity,"host executable exceeds the retained artifact bound"};
  std::ifstream input(file,std::ios::binary);Bytes bytes{std::istreambuf_iterator<char>(input),{}};
  if(!input||input.bad()||bytes.size()!=size)return {Code::io_error,"cannot read complete host executable"};
  const auto digest=sha256(bytes),module=p.catalog.describe().front().module_sha256;
  const auto key=sha256(pack({module,digest}));
  const auto payload=pack({"native-host-package/1",module,digest});
  ev::StoredRecord previous;const auto found=p.store->latest("sdk.host-installation",key,previous);
  if(found&&previous.record.payload!=payload)return {Code::conflict,"retained host package identity differs"};
  if(!found&&found.code!=Code::not_found)return found;
  if(!found) {
    ev::CommitBatch batch;batch.operation_id="host-package-"+key;batch.request_digest=sha256(payload);
    batch.guards.push_back({"sdk.host-installation",key,0,{}});
    batch.blobs.push_back({digest,std::move(bytes),{}});
    batch.records.push_back({"sdk.host-installation",key,payload,{digest},false});
    ev::CommitReceipt receipt;if(auto result=p.store->commit(batch,receipt);!result)return result;
  }
  life::Pin pin;pin.id="host-"+key;pin.owner="local-operator";pin.kind=life::PinKind::module;
  pin.generation=1;pin.objects={digest};
  return p.lifecycle->put_pin(pin,0,now(),"host-pin-"+key);
}
SessionService& NativeSession::runtime(){return *impl_->runtime;}
SessionRunner& NativeSession::runner(){return *impl_->runner;}
evidence::ICommitStore& NativeSession::store(){return *impl_->store;}
lifecycle::Service& NativeSession::lifecycle(){return *impl_->lifecycle;}
}

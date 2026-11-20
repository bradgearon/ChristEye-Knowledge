#include "sealed_index.hpp"
#include "native_store_filesystem.hpp"
#include "native_store_recovery.hpp"
#include "packed_index.hpp"
#include "anaiero/sdk/storage_tools.hpp"
#include "christeye/living/durable_io.hpp"
#include <atomic>
#include <array>
#include <chrono>
#include <mutex>
#include <set>
#include <random>
#include <utility>
#ifndef _WIN32
#include <sys/statvfs.h>
#endif

namespace anaiero::storage {
using namespace sdk::evidence;
namespace os=christeye::living::durable::os;
using fsops::require_file_capacity;
using fsops::measure_coexistence;
using fsops::directory;
namespace {
constexpr std::size_t decode_limit=64U*1024U*1024U;
constexpr auto index_coverage_name="INDEX-COVERAGE";
constexpr std::string_view operation_epoch_prefix="__epoch/";
constexpr std::size_t operation_logical_offset=operation_epoch_prefix.size()+17U;
bool valid_layout(IndexLayout layout){
  return layout==IndexLayout::automatic||layout==IndexLayout::single_node||layout==IndexLayout::packed_pages;
}
std::string layout_name(IndexLayout layout){
  return layout==IndexLayout::packed_pages?"native-packed-pages/1":"native-sealed-avl";
}
IndexLayout read_layout(std::string_view name){
  if(name=="native-packed-pages/1")return IndexLayout::packed_pages;
  if(name=="native-sealed-avl")return IndexLayout::single_node;
  throw Failure(Code::corrupt,"unknown derived index layout");
}
std::vector<std::string> compaction_intent(const fs::path& path){
  const auto raw=read_file(path,16384);std::vector<std::string> envelope;
  if(!unpack(raw,envelope,3,16384)||envelope.size()!=3)throw Failure(Code::corrupt,"invalid compacted layout intent");
  if(envelope[0]=="ANAIERO-COMPACTION-INTENT/1"){
    auto fields=unseal(raw,envelope[0],7);
    if(fields.size()!=7)throw Failure(Code::corrupt,"invalid compaction intent");
    fields.emplace_back("native-sealed-avl");return fields;
  }
  auto fields=unseal(raw,"ANAIERO-COMPACTION-INTENT/2",8);
  if(fields.size()!=8)throw Failure(Code::corrupt,"invalid compaction target intent");
  (void)read_layout(fields[7]);return fields;
}
using MonotonicClock=std::chrono::steady_clock;
std::uint64_t nanoseconds(MonotonicClock::duration duration)noexcept{
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
}
class CommitMeasurement {
  CommitTimings& result_;
  MonotonicClock::time_point started_{},phase_started_{};
  std::uint64_t CommitTimings::* phase_=&CommitTimings::validation_ns;
public:
  CommitMeasurement(CommitTimings& result,bool enabled):result_(result){
    result_={};result_.enabled=enabled;
    if(enabled)started_=phase_started_=MonotonicClock::now();
  }
  void phase(std::uint64_t CommitTimings::* next)noexcept{
    if(!result_.enabled)return;
    const auto now=MonotonicClock::now();
    result_.*phase_+=nanoseconds(now-phase_started_);phase_started_=now;phase_=next;
  }
  ~CommitMeasurement(){
    if(!result_.enabled)return;
    const auto now=MonotonicClock::now();
    result_.*phase_+=nanoseconds(now-phase_started_);result_.total_ns=nanoseconds(now-started_);
  }
};
bool legacy_index_exists(const fs::path& store){
  const auto legacy=store/"INDEX";
  std::error_code ec;const auto status=fs::symlink_status(legacy,ec);
  if(ec==std::errc::no_such_file_or_directory)return false;
  if(ec)throw Failure(Code::io_error,"cannot inspect legacy index coverage");
  // On a case-insensitive filesystem INDEX names the existing index directory,
  // not a legacy marker. Ignore only that verified alias; all other nonregular
  // paths and symlinks retain the storage adapter's strict rejection policy.
  if(fs::is_directory(status)){
    const bool alias=fs::equivalent(legacy,store/"index",ec);
    if(ec)throw Failure(Code::io_error,"cannot compare legacy index path");
    if(alias)return false;
  }
  return regular_exists(legacy);
}
#include "native_store_canonical_codec.inc"
}

class NativeStore final:public ICommitStore {
public:
  NativeStore(fs::path path,StoreOptions options):path_(std::move(path)),options_(options){}
  Result open();
  Result commit(const CommitBatch&,CommitReceipt&)override;
  Result lookup_operation(std::string_view,CommitReceipt&)override;
  Result latest(std::string_view,std::string_view,StoredRecord&)override;
  Result scan_latest(std::string_view,std::string_view,std::string_view,std::size_t,std::vector<StoredRecord>&,bool&)override;
  Result get_blob(std::string_view,Bytes&)override;
  Result blob_info(std::string_view,BlobInfo&)override;
  Result list_blobs(std::uint64_t,std::size_t,std::vector<BlobInfo>&,bool&)override;
  Result lookup_observation(std::string_view,std::uint64_t,std::uint64_t,ObservationClaim&)override;
  Result find_event(const EventKey&,StoredEvent&,std::string_view,std::string_view)override;
  Result reclaim_expired_blob(std::string_view,std::uint64_t)override;
  Result repair_index()override;
  Result audit(AuditReport&)override;
  StoreInfo info()const override;
  Result preview_compaction(const CompactionOptions&,CompactionPlan&);
  Result execute_compaction(const CompactionPlan&,CompactionReport&);
  Result resume_compaction(CompactionReport&);
  std::uint64_t generation()const noexcept override{return generation_.load();}
  void fault(FaultPoint value)noexcept{fault_.store(value);}
  IndexStatistics stats()const noexcept {std::lock_guard lock(mutex_);return index_?index_->stats:IndexStatistics{};}
  void reset_stats()noexcept{std::lock_guard lock(mutex_);if(index_)index_->stats={};}
  CommitTimings timings()const noexcept{std::lock_guard lock(mutex_);return timings_;}
private:
  fs::path path_;
  StoreOptions options_;
  CommitTimings timings_;
  mutable std::mutex mutex_;
  os::File writer_;
  Root root_;
  CanonicalBase base_;
  std::map<Digest,std::size_t> base_pages_;
  std::uint64_t pending_reclamation_bytes_=0;
  std::atomic<std::uint64_t> generation_{0};
  std::atomic<FaultPoint> fault_{FaultPoint::none};
  std::unique_ptr<SealedIndex> index_;
  std::string index_directory_="nodes-a";
  IndexLayout index_layout_=IndexLayout::single_node;
  Digest index_root_,index_canonical_;
  std::uint64_t index_generation_=0;
  std::uint64_t last_admitted_unix_s_=0;
  bool ready_=false,writable_=false;
  template<class F>Result read(F&& f){std::lock_guard lock(mutex_);try{require_ready();f();return {};}catch(const Failure& e){if(e.code==Code::corrupt){ready_=false;writable_=false;}return {e.code,e.what()};}catch(const std::exception& e){return {Code::io_error,e.what()};}}
  void require_ready()const{if(!ready_)throw Failure(Code::busy,"canonical store requires index repair before indexed reads/admission");}
  std::string value(std::string_view key,bool& found){return index_->get(index_root_,key,found);}
  bool record(std::string_view,std::string_view,StoredRecord&);
  StoredRecord indexed_record(std::string_view);
  bool blob(std::string_view,BlobInfo&);
  bool operation(std::string_view,CommitReceipt&);
  std::uint64_t reference_count(const Digest&);
  Transaction transaction(std::uint64_t);
  Digest prepare_packed(const Transaction&);
  void apply_index(const Transaction&,bool preserve_admission=false,const Digest* prepared=nullptr);
  void save_index();
  void load_index();
  void recover_staging();
  void cleanup_inactive_index();
  void verify_canonical_history();
  void audit_compacted(AuditReport&);
  void audit_locked(AuditReport&);
  void load_base();
  std::vector<std::pair<std::string,std::string>> base_page(const BasePage&);
  bool base_value(std::string_view,std::string&);
  std::string derived_base_value(const BasePage&,std::size_t,const std::pair<std::string,std::string>&);
  Digest build_base_index(SealedIndex&,const CanonicalBase&,bool compaction_build=false);
  void initialize_index_from_base();
  void cleanup_compaction();
  void fill_compaction_report(CompactionReport&,bool=false)const;
  void compacted_rows(bool,const std::function<void(const std::pair<std::string,std::string>&)>&,std::uint64_t&,std::uint64_t&);
  void preview_compaction_locked(const CompactionOptions&,CompactionPlan&,std::uint64_t* working_files=nullptr);
  void replay_suffix();
  void inject(FaultPoint point){if(fault_.load()==point){fault_.store(FaultPoint::none);throw Failure(Code::io_error,"injected canonical crash cut");}}
  Result commit_locked(const CommitBatch&,CommitReceipt&,std::string_view reclaim={});
  std::uint64_t usage()const;
};

StoredRecord NativeStore::indexed_record(std::string_view locator){
  const auto f=fields(locator,4);if(f.size()!=4||(f[0]!="record/1"&&f[0]!="record/2")||!valid_digest(f[3]))throw Failure(Code::corrupt,"invalid canonical record locator");
  if(f[0]=="record/2"){
    if(base_pages_.count(f[1])==0)throw Failure(Code::corrupt,"record locator is outside the canonical base");
    const auto& page=base_.pages[base_pages_.at(f[1])];const auto rows=base_page(page);const auto slot=parse_number(f[2]);if(slot>=rows.size())throw Failure(Code::corrupt,"base record slot is absent");const auto& row=rows[static_cast<std::size_t>(slot)];if(row.first.front()!='r'&&row.first.front()!='h')throw Failure(Code::corrupt,"base locator does not name a record");auto record=decode_record(row.second);if(record.digest!=f[3]||record.generation>base_.anchor.generation)throw Failure(Code::corrupt,"base record identity mismatch");return record;
  }
  const auto tx=transaction(parse_number(f[1]));const auto slot=parse_number(f[2]);
  if(slot>=tx.records.size()||tx.records[static_cast<std::size_t>(slot)].digest!=f[3])throw Failure(Code::corrupt,"canonical record locator has no matching witness");
  return tx.records[static_cast<std::size_t>(slot)];
}
bool NativeStore::record(std::string_view kind,std::string_view key,StoredRecord& out){bool found=false;const auto raw=value(record_key(kind,key),found);if(found){out=indexed_record(raw);if(out.record.kind!=kind||out.record.key!=key)throw Failure(Code::corrupt,"record index key mismatch");}return found;}
bool NativeStore::blob(std::string_view digest,BlobInfo& out){bool found=false;const auto raw=value("b"+std::string(digest),found);if(found)out=decode_blob(raw);return found;}
bool NativeStore::operation(std::string_view id,CommitReceipt& out){bool found=false;const auto raw=value("p"+std::string(id),found);if(found)out=decode_receipt(raw);return found;}
std::uint64_t NativeStore::reference_count(const Digest& digest){bool found=false;const auto raw=value("f"+digest,found);if(!found)return 0;const auto v=fields(raw,2);if(v.size()!=2)throw Failure(Code::corrupt,"invalid reference index");return parse_number(v[1]);}
Transaction NativeStore::transaction(std::uint64_t generation){
  const auto path=path_/"transactions"/(number(generation)+".txn");
  if(!regular_exists(path))throw Failure(Code::corrupt,"sealed canonical transaction is missing");
  const auto raw=read_file(path,decode_limit);auto t=decode_transaction(raw);
  if(t.root.generation!=generation||t.root.store!=root_.store)throw Failure(Code::corrupt,"canonical transaction scope mismatch");
  return t;
}
void NativeStore::save_index(){
  std::vector<std::string> fields{root_.store,index_directory_,number(index_generation_),index_canonical_,index_root_,root_.base};
  if(index_layout_==IndexLayout::packed_pages){
    fields.push_back(layout_name(index_layout_));
    write_file(path_/index_coverage_name,seal("ANAIERO-INDEX-COVERAGE/4",fields));
  }else write_file(path_/index_coverage_name,seal("ANAIERO-INDEX-COVERAGE/3",fields));
}
template<class Put>
void emit_index_updates(const Transaction& t,const Put& put){
  for(std::size_t slot=0;slot<t.records.size();++slot){const auto& r=t.records[slot];const auto locator=pack({"record/1",number(t.root.generation),number(slot),r.digest});put(record_key(r.record.kind,r.record.key),locator);put(history_key(r),locator);}
  for(const auto& b:t.blobs){put("b"+b.digest,encode_blob(b));put("v"+number(b.ordinal),b.digest);}
  // Only the final source tip is visible in this transaction's covered root.
  // Coalesce repeated updates before path copying; every strict claim retains
  // its own indexed identity and canonical witness. The transaction's bounded
  // observation count also bounds this temporary map.
  std::map<std::string,std::string> tips;
  for(const auto& c:t.observations){const auto payload=pack({number(t.root.generation),encode_claim(c)});put(observation_key(c),payload);tips["t"+observation_prefix(c.source_uid,c.epoch)]=payload;}
  for(const auto& tip:tips)put(tip.first,tip.second);
  for(const auto& e:t.events){put("e"+event_tuple(e.event.key),encode_event(e));put(exact_event_key(e.event),encode_event(e));}
  for(const auto& ref:t.references)put("f"+ref.first,pack({number(t.root.generation),number(ref.second)}));
  put("p"+t.receipt.operation_id,encode_receipt(t.receipt));
}
Digest NativeStore::prepare_packed(const Transaction& t){
  std::map<std::string,std::string> updates;
  emit_index_updates(t,[&](const std::string& key,const std::string& data){updates[key]=data;});
  return index_->prepare_packed(index_root_,updates);
}
void NativeStore::apply_index(const Transaction& t,bool preserve_admission,const Digest* prepared){
  if(t.root.generation!=index_generation_+1||t.predecessor!=index_canonical_)throw Failure(Code::corrupt,"noncontiguous canonical/index transition");
  if(preserve_admission)index_->begin_publication();else index_->begin();
  auto next=index_root_;
  if(index_->packed())next=prepared?*prepared:prepare_packed(t);
  else emit_index_updates(t,[&](const std::string& key,const std::string& data){next=index_->put(next,key,data);});
  index_->publish_cleanup(next);
  index_root_=next;index_generation_=t.root.generation;index_canonical_=t.root.digest;
  save_index();index_->finish_cleanup(index_root_);
}
std::uint64_t NativeStore::usage()const{
  std::uint64_t index_bytes=0;
  if(index_&&!index_root_.empty()){try{index_bytes=index_->load(index_root_).bytes;}catch(...){return options_.maximum_bytes;}}
  return root_.canonical_bytes-base_.anchor.canonical_bytes+base_.storage_bytes+root_.blob_bytes+index_bytes+pending_reclamation_bytes_+16384;
}
StoreInfo NativeStore::info()const{
  std::lock_guard lock(mutex_);StoreInfo i;i.index_backend=layout_name(index_layout_);i.store_id=root_.store;i.generation=root_.generation;i.index_generation=index_generation_;i.blob_count=root_.blobs;i.record_count=root_.records;i.physical_bytes=usage();i.maximum_bytes=options_.maximum_bytes;i.maximum_batch_bytes=options_.maximum_batch_bytes;i.maintenance_reserve_bytes=std::min(options_.maintenance_reserve_bytes,options_.maximum_bytes/8U);i.ordinary_limit_bytes=options_.maximum_bytes-i.maintenance_reserve_bytes;i.root_digest=root_.digest;i.index_root=index_root_;i.writable=writable_;i.index_ready=ready_;i.canonical_base_generation=base_.anchor.generation;i.canonical_base_digest=root_.base;i.operation_epoch=base_.operation_epoch;i.admitted_unix_s=last_admitted_unix_s_;i.pending_reclamation_bytes=pending_reclamation_bytes_;return i;
}
Result NativeStore::commit(const CommitBatch& batch,CommitReceipt& receipt){
  const auto waiting=options_.measure_commit_stages?MonotonicClock::now():MonotonicClock::time_point{};
  std::lock_guard lock(mutex_);
  const auto acquired=options_.measure_commit_stages?MonotonicClock::now():MonotonicClock::time_point{};
  const auto result=commit_locked(batch,receipt);
  if(options_.measure_commit_stages){timings_.lock_wait_ns=nanoseconds(acquired-waiting);timings_.total_ns+=timings_.lock_wait_ns;}
  return result;
}
Result NativeStore::commit_locked(const CommitBatch& batch,CommitReceipt& receipt,std::string_view reclaim){
  CommitMeasurement measurement(timings_,options_.measure_commit_stages);
  bool published=false,staged=false;receipt={};
  try{
    require_ready();if(!writable_)throw Failure(Code::busy,"canonical admission is stopped");
    // Admission revisits shared paths for guards, object presence and source
    // sequence checks. Cache only within this serialized operation; copied
    // reads and future operations never inherit a stale in-process cache.
    index_->begin();struct CacheScope {SealedIndex& index;~CacheScope(){index.discard_cache();}} cache_scope{*index_};
    if(!text_key(batch.operation_id,256)||!valid_digest(batch.request_digest))throw Failure(Code::invalid,"invalid operation identity");
    if(batch.commit_class!=CommitClass::ordinary&&batch.commit_class!=CommitClass::maintenance)throw Failure(Code::invalid,"invalid commit class");
    if(reclaim.empty()&&(batch.operation_id.starts_with("__storage/")||(batch.operation_id.starts_with(operation_epoch_prefix)&&batch.operation_id.size()>24&&std::string_view(batch.operation_id).substr(24).starts_with("__storage/"))))throw Failure(Code::invalid,"operation ID uses a reserved storage namespace");
    if(batch.records.size()>options_.maximum_records||batch.guards.size()>options_.maximum_records*4||batch.blobs.size()>options_.maximum_blobs||batch.observations.size()>options_.maximum_records||batch.events.size()>options_.maximum_records||batch.expire_blobs.size()>options_.maximum_blobs||batch.authorizations.size()>16)throw Failure(Code::capacity,"canonical batch count bound exceeded");
    std::size_t request_bytes=0;
    const auto account=[&](std::size_t bytes){if(bytes>options_.maximum_batch_bytes-request_bytes)throw Failure(Code::capacity,"canonical batch byte bound exceeded");request_bytes+=bytes;};
    account(batch.operation_id.size());account(batch.request_digest.size());
    for(const auto& r:batch.records){
      if(!text_key(r.kind,128)||!text_key(r.key)||r.references.size()>options_.maximum_references)throw Failure(Code::invalid,"invalid bounded record identity/references");
      account(r.kind.size());account(r.key.size());account(r.payload.size());
      for(const auto& ref:r.references){if(!valid_digest(ref))throw Failure(Code::invalid,"invalid record reference digest");account(ref.size());}
    }
    for(const auto& b:batch.blobs){
      if(!valid_digest(b.digest)||b.bytes.size()>options_.maximum_blob_bytes||b.references.size()>options_.maximum_references)throw Failure(Code::invalid,"invalid bounded blob identity/references");
      account(b.digest.size());account(b.bytes.size());for(const auto& ref:b.references){if(!valid_digest(ref))throw Failure(Code::invalid,"invalid blob reference digest");account(ref.size());}
    }
    for(const auto& g:batch.guards){if(!text_key(g.kind,128)||!text_key(g.key)||(!g.expected_digest.empty()&&!valid_digest(g.expected_digest)))throw Failure(Code::invalid,"invalid record fence");account(g.kind.size());account(g.key.size());account(g.expected_digest.size());account(8);}
    for(const auto& c:batch.observations){if(!text_key(c.source_uid,256)||!valid_digest(c.payload_digest)||!valid_digest(c.receipt_digest))throw Failure(Code::invalid,"invalid observation identity");account(c.source_uid.size());account(144);}
    for(const auto& e:batch.events){if(!text_key(e.key.kind,128)||!text_key(e.key.entity,256)||!text_key(e.key.clock,256)||!text_key(e.key.producer,256)||!valid_digest(e.object_digest)||e.origin.size()>4096)throw Failure(Code::invalid,"invalid generic event identity");account(e.key.kind.size());account(e.key.entity.size());account(e.key.clock.size());account(e.key.producer.size());account(e.object_digest.size());account(e.origin.size());account(8);}
    for(const auto& digest:batch.expire_blobs){if(!valid_digest(digest))throw Failure(Code::invalid,"invalid expiry digest");account(digest.size());}
    for(const auto& a:batch.authorizations){if(!text_key(a.principal,256)||!text_key(a.scope,1024)||a.credential_generation==0)throw Failure(Code::invalid,"invalid admission authorization");account(a.principal.size());account(a.scope.size());account(16);}
    const auto semantic_digest=reclaim.empty()?batch_digest(batch):sha256(pack({"ANAIERO-RECLAIM/1",std::string(reclaim)}));
    if(operation(batch.operation_id,receipt)){
      if(receipt.request_digest!=batch.request_digest||receipt.batch_digest!=semantic_digest)throw Failure(Code::conflict,"operation ID was already committed with different content");
      receipt.duplicate=true;timings_.duplicate=true;timings_.committed=true;timings_.generation=receipt.generation;return {};
    }
    const auto request_epoch=operation_epoch(batch.operation_id);
    if(request_epoch<base_.operation_epoch)throw Failure(Code::history_expired,"unknown operation predates the explicit receipt-history epoch; do not relabel a retry");
    if(request_epoch>base_.operation_epoch)throw Failure(Code::conflict,"operation epoch has not been activated");
    if(batch.expected_generation!=any_generation&&batch.expected_generation!=root_.generation)throw Failure(Code::conflict,"canonical generation fence changed");
    if(root_.generation==any_generation)throw Failure(Code::capacity,"canonical generation exhausted");
    for(const auto& guard:batch.guards){StoredRecord previous;const bool exists=record(guard.kind,guard.key,previous);if((exists?previous.revision:0)!=guard.expected_revision||(!guard.expected_digest.empty()&&(!exists||previous.digest!=guard.expected_digest)))throw Failure(Code::conflict,"record revision/digest fence changed");}
    Transaction tx;tx.root=root_;++tx.root.generation;tx.root.digest.clear();tx.predecessor=root_.digest;tx.receipt={batch.operation_id,batch.request_digest,semantic_digest,"",tx.root.generation,false};tx.receipt.authorizations=batch.authorizations;tx.receipt.commit_class=batch.commit_class;tx.guards=batch.guards;tx.expected_generation=batch.expected_generation;
    std::map<Digest,BlobInfo> changed_blobs;
    std::map<Digest,std::uint64_t> counts;
    const auto adjust=[&](const Digest& digest,bool add){auto [it,inserted]=counts.try_emplace(digest,0);if(inserted)it->second=reference_count(digest);if(add){if(it->second==any_generation)throw Failure(Code::capacity,"reference counter exhausted");++it->second;}else{if(it->second==0)throw Failure(Code::corrupt,"reference count underflow");--it->second;}};
    const auto available=[&](const Digest& digest){BlobInfo info;const auto changed=changed_blobs.find(digest);if(changed!=changed_blobs.end())info=changed->second;else if(!blob(digest,info))throw Failure(Code::not_found,"referenced blob has not been committed");if(info.expired_generation!=0)throw Failure(Code::history_expired,"referenced blob has expired");if(!info.present||(info.created_generation!=tx.root.generation&&(!regular_exists(path_/"blobs"/digest)||fs::file_size(path_/"blobs"/digest)!=info.bytes)))throw Failure(Code::corrupt,"available blob is physically absent or truncated");};
    std::set<Digest> batch_blobs;
    std::vector<const Blob*> new_blobs;
    for(const auto& b:batch.blobs){
      if(!valid_digest(b.digest)||sha256(b.bytes)!=b.digest||b.bytes.size()>options_.maximum_blob_bytes||b.references.size()>options_.maximum_references||!batch_blobs.insert(b.digest).second)throw Failure(Code::invalid,"invalid or repeated batch blob");
      std::set<Digest> unique;for(const auto& ref:b.references){if(!valid_digest(ref)||ref==b.digest||!unique.insert(ref).second)throw Failure(Code::invalid,"invalid blob graph edge");available(ref);}
      BlobInfo previous;if(blob(b.digest,previous)){if(previous.expired_generation!=0)throw Failure(Code::history_expired,"expired blob identity cannot be silently resurrected");if(previous.bytes!=b.bytes.size()||previous.references!=b.references)throw Failure(Code::conflict,"blob identity has different metadata");if(read_file(path_/"blobs"/b.digest,options_.maximum_blob_bytes)!=b.bytes)throw Failure(Code::corrupt,"existing blob content mismatch");continue;}
      BlobInfo next{b.digest,++tx.root.blobs,static_cast<std::uint64_t>(b.bytes.size()),tx.root.generation,0,true,b.references};changed_blobs.emplace(b.digest,next);tx.root.blob_bytes+=next.bytes;new_blobs.push_back(&b);for(const auto& ref:b.references)adjust(ref,true);
    }
    std::set<std::string> changed_records;
    for(const auto& r:batch.records){
      if(!text_key(r.kind,128)||!text_key(r.key)||r.kind=="__storage"||r.references.size()>options_.maximum_references||!changed_records.insert(record_key(r.kind,r.key)).second||(r.tombstone&&!r.references.empty()))throw Failure(Code::invalid,"invalid, duplicate, or reserved record mutation");
      std::set<Digest> unique;for(const auto& ref:r.references){if(!valid_digest(ref)||!unique.insert(ref).second)throw Failure(Code::invalid,"invalid record root reference");available(ref);}
      StoredRecord previous;const bool exists=record(r.kind,r.key,previous);if(exists&&previous.revision==any_generation)throw Failure(Code::capacity,"record revision exhausted");
      if(exists&&!previous.record.tombstone)for(const auto& ref:previous.record.references)adjust(ref,false);
      if(!r.tombstone)for(const auto& ref:r.references)adjust(ref,true);
      tx.records.push_back({r,exists?previous.revision+1:1,tx.root.generation,record_digest(r)});if(!exists)++tx.root.records;
    }
    std::set<Digest> expired;
    for(const auto& digest:batch.expire_blobs){
      if(!valid_digest(digest)||!expired.insert(digest).second)throw Failure(Code::invalid,"invalid duplicate expiry identity");
      BlobInfo b;if(changed_blobs.count(digest)!=0)throw Failure(Code::invalid,"cannot create and expire a blob in one batch");if(!blob(digest,b))throw Failure(Code::not_found,"expiry blob absent");
      if(b.expired_generation!=0)continue;
      b.expired_generation=tx.root.generation;changed_blobs[digest]=b;for(const auto& ref:b.references)adjust(ref,false);
    }
    for(const auto& digest:expired){const auto c=counts.find(digest);if((c==counts.end()?reference_count(digest):c->second)!=0)throw Failure(Code::conflict,"blob is protected by a current root or live blob edge");}
    if(!reclaim.empty()){
      BlobInfo b;if(!blob(reclaim,b))throw Failure(Code::not_found,"reclaim blob absent");if(b.expired_generation==0)throw Failure(Code::conflict,"reclamation requires canonical expiry");if(reference_count(b.digest)!=0)throw Failure(Code::corrupt,"expired blob retains a live root");
      if(b.present){b.present=false;if(tx.root.blob_bytes<b.bytes)throw Failure(Code::corrupt,"physical byte counter underflow");tx.root.blob_bytes-=b.bytes;changed_blobs[b.digest]=b;}
    }
    std::map<std::string,ObservationClaim> tips;
    std::set<std::string> new_claims;
    for(const auto& c:batch.observations){
      if(!text_key(c.source_uid,256)||c.sequence==0||!valid_digest(c.payload_digest)||!valid_digest(c.receipt_digest))throw Failure(Code::invalid,"invalid strict observation claim");
      if(base_.counts['q']!=0){bool below=false;const auto floor=value("q"+observation_prefix(c.source_uid,c.epoch),below);if(below){const auto f=fields(floor,2);if(f.size()!=2)throw Failure(Code::corrupt,"invalid observation floor");if(c.sequence<=decode_claim(f[1]).sequence)throw Failure(Code::history_expired,"strict observation is below the canonical history floor");}}
      const auto key=observation_key(c);if(!new_claims.insert(key).second)throw Failure(Code::invalid,"repeated strict observation in batch");
      bool exists=false;const auto existing=value(key,exists);if(exists){const auto f=fields(existing,2);if(f.size()!=2||f[1]!=encode_claim(c))throw Failure(Code::conflict,"source sequence already has different evidence");continue;}
      available(c.payload_digest);available(c.receipt_digest);
      const auto tipkey="t"+observation_prefix(c.source_uid,c.epoch);auto tip=tips.find(tipkey);
      if(tip==tips.end()){bool present=false;const auto old=value(tipkey,present);ObservationClaim prior;if(present){const auto f=fields(old,2);if(f.size()!=2)throw Failure(Code::corrupt,"invalid source tip index");prior=decode_claim(f[1]);}tip=tips.emplace(tipkey,std::move(prior)).first;}
      if(tip->second.sequence==any_generation||c.sequence!=tip->second.sequence+1)throw Failure(Code::conflict,"strict source sequence is out of order");
      tip->second=c;tx.observations.push_back(c);
    }
    std::set<std::string> exact_events;
    for(const auto& event:batch.events){
      if(!text_key(event.key.kind,128)||!text_key(event.key.entity,256)||!text_key(event.key.clock,256)||!text_key(event.key.producer,256)||!valid_digest(event.object_digest)||event.origin.size()>4096)throw Failure(Code::invalid,"invalid generic registry event");
      const auto key=exact_event_key(event);bool exists=false;(void)value(key,exists);if(exists||!exact_events.insert(key).second)continue;available(event.object_digest);
      tx.events.push_back({event,tx.root.generation,static_cast<std::uint64_t>(tx.events.size()),sha256(pack({event_tuple(event.key),event.object_digest,event.origin}))});
    }
    for(const auto& b:changed_blobs)tx.blobs.push_back(b.second);
    for(const auto& c:counts)tx.references.push_back(c);
    auto raw=encode_transaction(tx);tx.root.canonical_bytes+=static_cast<std::uint64_t>(raw.size());raw=encode_transaction(tx);
    tx.root.digest=sha256(raw);tx.receipt.root_digest=tx.root.digest;
    // Reserve the entire batch and a conservative bounded COW path workspace.
    // Filesystem free-space checks also cover allocation-unit/metadata costs.
    const auto index_head=index_->load(index_root_);const auto node_height=index_head.height;
    const auto mutations=tx.records.size()*2U+tx.blobs.size()*2U+tx.observations.size()*2U+tx.events.size()*2U+tx.references.size()+1U;
    if(mutations>options_.maximum_index_updates)throw Failure(Code::capacity,"bounded index update budget exceeded; split independent maintenance work");
    std::uint64_t largest_node=std::max<std::uint64_t>(index_head.maximum_node_bytes,4096);
    for(const auto& b:tx.blobs)largest_node=std::max(largest_node,static_cast<std::uint64_t>(encode_blob(b).size())+1024U);
    for(const auto& e:tx.events)largest_node=std::max(largest_node,static_cast<std::uint64_t>(encode_event(e).size()+exact_event_key(e.event).size())+1024U);
    // Four nodes per level covers path copying plus double rotations. Heights
    // can rise by at most log2(number of additions)+1 within the bounded batch.
    std::uint64_t height_growth=1;for(auto n=mutations;n>1;n=(n+1U)/2U)++height_growth;
    const auto canonical_reserve=static_cast<std::uint64_t>(request_bytes)*2U+static_cast<std::uint64_t>(raw.size())*2U+32768U;
    const auto reserve=canonical_reserve+(index_->packed()?0U:static_cast<std::uint64_t>(mutations)*(node_height+height_growth+2U)*largest_node*4U);
    const auto reclaimed_bytes=reclaim.empty()?0:root_.blob_bytes-tx.root.blob_bytes;
    const auto accounted=usage();if(reclaimed_bytes>accounted)throw Failure(Code::corrupt,"reclamation exceeds accounted storage bytes");const auto used=accounted-reclaimed_bytes;
    const auto maintenance_reserve=std::min(options_.maintenance_reserve_bytes,options_.maximum_bytes/8U);
    const auto budget=batch.commit_class==CommitClass::maintenance?options_.maximum_bytes:options_.maximum_bytes-maintenance_reserve;
    std::error_code space_error;const auto space=fs::space(path_,space_error);
    if(used>budget||reserve>budget-used)throw Failure(Code::capacity,batch.commit_class==CommitClass::maintenance?"finite maintenance headroom exhausted; absolute byte quota still applies":"ordinary byte budget preserves maintenance headroom; retain less or review capacity");
    if(space_error||space.available<options_.minimum_free_bytes||reserve>space.available-options_.minimum_free_bytes)throw Failure(Code::capacity,"filesystem admission headroom exhausted");
    const auto temporary_files=static_cast<std::uint64_t>(mutations)*(node_height+height_growth+2U)*4U+
      static_cast<std::uint64_t>(new_blobs.size())+16U;
    if(!index_->packed())require_file_capacity(path_,temporary_files,options_.minimum_free_file_nodes);
    const auto system_seconds=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto admitted=options_.admission_clock?options_.admission_clock->unix_seconds():(system_seconds<0?0:static_cast<std::uint64_t>(system_seconds));
    if(admitted<last_admitted_unix_s_)throw Failure(Code::expired,"trusted admission clock precedes the durable time floor");
    for(const auto& a:batch.authorizations)if(a.not_after_unix_s!=0&&admitted>=a.not_after_unix_s)throw Failure(Code::expired,"authorization expired before serialized commit admission");
    tx.receipt.admitted_unix_s=admitted;raw=encode_transaction(tx);tx.root.digest=sha256(raw);tx.receipt.root_digest=tx.root.digest;
    Digest prepared;
    if(index_->packed()){
      measurement.phase(&CommitTimings::index_ns);
      prepared=prepare_packed(tx);
      const auto working=index_->packed_work_bytes();
      if(working>budget-used-reserve||working>space.available-options_.minimum_free_bytes-reserve)
        throw Failure(Code::capacity,"packed index publication requires additional working headroom");
      require_file_capacity(path_,index_->packed_work_files()+static_cast<std::uint64_t>(new_blobs.size())+16U,options_.minimum_free_file_nodes);
    }
    measurement.phase(&CommitTimings::staging_ns);
    std::vector<std::string> stage{root_.store,number(tx.root.generation),root_.digest};for(const auto* b:new_blobs)stage.push_back(b->digest);write_file(path_/"STAGING",seal("ANAIERO-STAGING/1",stage));staged=true;
    measurement.phase(&CommitTimings::blobs_ns);
    for(const auto* b:new_blobs)write_file(path_/"blobs"/b->digest,b->bytes);
    inject(FaultPoint::after_objects);
    measurement.phase(&CommitTimings::transaction_ns);
    write_file(path_/"transactions"/(number(tx.root.generation)+".txn"),raw);inject(FaultPoint::after_transaction);
    measurement.phase(&CommitTimings::root_ns);
    write_file(path_/"ROOT",encode_root(tx.root));root_=tx.root;generation_.store(root_.generation);last_admitted_unix_s_=admitted;receipt=tx.receipt;published=true;
    timings_.committed=true;timings_.generation=receipt.generation;
    inject(FaultPoint::after_root);inject(FaultPoint::before_index);
    measurement.phase(&CommitTimings::index_ns);apply_index(tx,true,index_->packed()?&prepared:nullptr);inject(FaultPoint::after_index);
    measurement.phase(&CommitTimings::cleanup_ns);
    remove_file(path_/"STAGING");return {};
  }catch(const Failure& e){if(staged){writable_=false;if(published)ready_=false;}if(e.code==Code::corrupt){ready_=false;writable_=false;}timings_.result=published?Code::committed_degraded:e.code;return {timings_.result,e.what()};}
  catch(const std::exception& e){if(staged){writable_=false;if(published)ready_=false;}timings_.result=published?Code::committed_degraded:Code::io_error;return {timings_.result,e.what()};}
}

Result NativeStore::lookup_operation(std::string_view id,CommitReceipt& out){return read([&]{if(!operation(id,out)){const auto epoch=operation_epoch(id);if(epoch<base_.operation_epoch)throw Failure(Code::history_expired,"operation receipt is below the explicit retry-history epoch");if(epoch>base_.operation_epoch)throw Failure(Code::conflict,"operation epoch has not been activated");throw Failure(Code::not_found,"operation receipt absent");}});}
Result NativeStore::latest(std::string_view kind,std::string_view key,StoredRecord& out){return read([&]{if(!record(kind,key,out))throw Failure(Code::not_found,"record was never created");});}
Result NativeStore::scan_latest(std::string_view kind,std::string_view prefix,std::string_view after,std::size_t limit,std::vector<StoredRecord>& out,bool& more){return read([&]{
  if(!text_key(kind,128)||limit==0||limit>1024)throw Failure(Code::invalid,"invalid bounded record scan");
  std::vector<std::pair<std::string,std::string>> rows;const auto base=record_key(kind,prefix);index_->scan(index_root_,base,after.empty()?std::string{}:record_key(kind,after),limit,rows,more);out.clear();for(const auto& row:rows){auto r=indexed_record(row.second);if(record_key(r.record.kind,r.record.key)!=row.first)throw Failure(Code::corrupt,"record range index key mismatch");out.push_back(std::move(r));}
});}
Result NativeStore::get_blob(std::string_view digest,Bytes& out){return read([&]{
  if(!valid_digest(digest))throw Failure(Code::invalid,"invalid blob identity");
  BlobInfo b;if(!blob(digest,b))throw Failure(Code::not_found,"blob metadata absent");if(b.expired_generation!=0)throw Failure(Code::history_expired,"blob payload has canonically expired");
  if(!regular_exists(path_/"blobs"/b.digest))throw Failure(Code::corrupt,"unexpired blob payload missing");
  out=read_file(path_/"blobs"/b.digest,options_.maximum_blob_bytes);if(out.size()!=b.bytes||sha256(out)!=b.digest)throw Failure(Code::corrupt,"blob verification failed");
});}
Result NativeStore::blob_info(std::string_view digest,BlobInfo& out){return read([&]{if(!valid_digest(digest))throw Failure(Code::invalid,"invalid blob identity");if(!blob(digest,out))throw Failure(Code::not_found,"blob metadata absent");out.present=regular_exists(path_/"blobs"/out.digest);if(!out.present&&out.expired_generation==0)throw Failure(Code::corrupt,"unexpired blob payload missing");});}
Result NativeStore::list_blobs(std::uint64_t after,std::size_t limit,std::vector<BlobInfo>& out,bool& more){return read([&]{
  if(limit==0||limit>1024)throw Failure(Code::invalid,"invalid bounded blob scan");
  std::vector<std::pair<std::string,std::string>> rows;index_->scan(index_root_,"v",after==0?std::string{}:"v"+number(after),limit,rows,more);out.clear();for(const auto& row:rows){BlobInfo b;if(!blob(row.second,b))throw Failure(Code::corrupt,"blob ordinal index points to missing metadata");b.present=regular_exists(path_/"blobs"/b.digest);if(!b.present&&b.expired_generation==0)throw Failure(Code::corrupt,"unexpired blob missing");out.push_back(std::move(b));}
});}
Result NativeStore::lookup_observation(std::string_view source,std::uint64_t epoch,std::uint64_t sequence,ObservationClaim& out){return read([&]{
  if(base_.counts['q']!=0){bool closed=false;const auto floor=value("q"+observation_prefix(source,epoch),closed);if(closed){const auto f=fields(floor,2);if(f.size()!=2)throw Failure(Code::corrupt,"invalid observation expiry floor");if(sequence<=decode_claim(f[1]).sequence)throw Failure(Code::history_expired,"observation is below the compacted canonical history floor");}}
  bool found=false;const auto raw=value("o"+observation_prefix(source,epoch)+number(sequence),found);if(!found)throw Failure(Code::not_found,"observation claim absent");const auto f=fields(raw,2);if(f.size()!=2)throw Failure(Code::corrupt,"invalid observation index entry");out=decode_claim(f[1]);
});}
Result NativeStore::find_event(const EventKey& key,StoredEvent& out,std::string_view exact_object,std::string_view exact_origin){return read([&]{
  bool found=false;const auto indexed=exact_object.empty()?"e"+event_tuple(key):exact_event_key({key,std::string(exact_object),std::string(exact_origin)});const auto raw=value(indexed,found);if(!found)throw Failure(Code::not_found,"generic registry event absent");out=decode_event(raw);
});}
Result NativeStore::reclaim_expired_blob(std::string_view digest,std::uint64_t expected){
  std::lock_guard lock(mutex_);
  try{
    require_ready();if(expected!=root_.generation)throw Failure(Code::conflict,"reclamation generation fence changed");if(!valid_digest(digest))throw Failure(Code::invalid,"invalid reclaim identity");BlobInfo b;if(!blob(digest,b))throw Failure(Code::not_found,"reclaim blob absent");if(b.expired_generation==0||reference_count(b.digest)!=0)throw Failure(Code::conflict,"reclamation requires unprotected canonical expiry");
    if(!b.present)return {};
    remove_file(path_/"blobs"/b.digest);
    CommitBatch batch;batch.operation_id=qualify_operation_id(base_.operation_epoch,"__storage/reclaim/"+b.digest+'/'+number(b.expired_generation));batch.request_digest=sha256(pack({b.digest,number(b.expired_generation)}));batch.expected_generation=expected;batch.commit_class=CommitClass::maintenance;CommitReceipt receipt;return commit_locked(batch,receipt,digest);
  }catch(const Failure& e){return {e.code,e.what()};}catch(const std::exception& e){return {Code::io_error,e.what()};}
}
#include "native_store_compaction_impl.inc"
#include "native_store_recovery_impl.inc"
}

namespace anaiero::sdk::evidence {
Result open_native_store(std::string_view directory,const StoreOptions& options,std::unique_ptr<ICommitStore>& out){
  out.reset();try{const auto utf8=std::u8string(reinterpret_cast<const char8_t*>(directory.data()),directory.size());auto store=std::make_unique<storage::NativeStore>(std::filesystem::path(utf8),options);auto result=store->open();if(result)out=std::move(store);return result;}catch(const std::exception& e){return {Code::io_error,e.what()};}
}
void set_native_store_fault(ICommitStore& store,FaultPoint fault)noexcept{if(auto* native=dynamic_cast<storage::NativeStore*>(&store))native->fault(fault);}
IndexStatistics native_index_statistics(const ICommitStore& store)noexcept{if(const auto* native=dynamic_cast<const storage::NativeStore*>(&store))return native->stats();return {};}
void reset_native_index_statistics(ICommitStore& store)noexcept{if(auto* native=dynamic_cast<storage::NativeStore*>(&store))native->reset_stats();}
CommitTimings native_commit_timings(const ICommitStore& store)noexcept{if(const auto* native=dynamic_cast<const storage::NativeStore*>(&store))return native->timings();return {};}
Result preview_native_compaction(ICommitStore& store,const CompactionOptions& options,CompactionPlan& plan){if(auto* native=dynamic_cast<storage::NativeStore*>(&store))return native->preview_compaction(options,plan);return {Code::unsupported,"compaction requires the native canonical store"};}
Result execute_native_compaction(ICommitStore& store,const CompactionPlan& plan,CompactionReport& report){if(auto* native=dynamic_cast<storage::NativeStore*>(&store))return native->execute_compaction(plan,report);return {Code::unsupported,"compaction requires the native canonical store"};}
Result resume_native_compaction(ICommitStore& store,CompactionReport& report){if(auto* native=dynamic_cast<storage::NativeStore*>(&store))return native->resume_compaction(report);return {Code::unsupported,"compaction requires the native canonical store"};}
std::string qualify_operation_id(std::uint64_t epoch,std::string_view logical_id){if(epoch==0)return std::string(logical_id);return std::string(storage::operation_epoch_prefix)+storage::number(epoch)+"/"+std::string(logical_id);}

Result inspect_native_operation_epoch(std::string_view directory,std::uint64_t& epoch){
  epoch=0;
  try{
    const auto utf8=std::u8string(reinterpret_cast<const char8_t*>(directory.data()),directory.size());const auto path=std::filesystem::path(utf8);
    const auto root=storage::decode_root(storage::read_file(path/"ROOT",16384));
    if(root.base.empty())return {};
    const auto status=std::filesystem::symlink_status(path/"bases");if(!std::filesystem::is_directory(status)||std::filesystem::is_symlink(status))return {Code::corrupt,"invalid canonical base directory"};
    const auto bytes=storage::read_file(path/"bases"/(root.base+".base"),storage::decode_limit);if(sha256(bytes)!=root.base)return {Code::corrupt,"canonical base identity mismatch"};
    const auto base=storage::decode_base(bytes);if(base.anchor.store!=root.store||base.anchor.generation>root.generation||(base.anchor.generation==root.generation&&!storage::same_root_witness(base.anchor,root)))return {Code::corrupt,"canonical base scope mismatch"};
    epoch=base.operation_epoch;return {};
  }catch(const storage::Failure& e){return {e.code,e.what()};}catch(const std::exception& e){return {Code::io_error,e.what()};}
}

}

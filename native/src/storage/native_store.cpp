#include "sealed_index.hpp"
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
namespace {
constexpr std::size_t decode_limit=64U*1024U*1024U;
constexpr auto index_coverage_name="INDEX-COVERAGE";
constexpr std::string_view operation_epoch_prefix="__epoch/";
constexpr std::size_t operation_logical_offset=operation_epoch_prefix.size()+17U;
void require_file_capacity(const fs::path& path,std::uint64_t temporary,std::uint64_t reserve){
#ifndef _WIN32
  struct statvfs status{};
  if(statvfs(path.c_str(),&status)!=0)throw Failure(Code::io_error,"cannot measure filesystem file-node headroom");
  const auto available=static_cast<std::uint64_t>(status.f_favail);
  if(status.f_files!=0&&available!=any_generation&&
     (available<reserve||temporary>available-reserve))
    throw Failure(Code::capacity,"filesystem file-node admission headroom exhausted; free bytes do not imply free file entries");
#else
  (void)path;(void)temporary;(void)reserve;
#endif
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
std::vector<std::string> fields(std::string_view bytes,std::size_t max=100000){
  std::vector<std::string> f;if(!unpack(bytes,f,max,decode_limit))throw Failure(Code::corrupt,"invalid canonical field list");return f;
}
bool text_key(std::string_view v,std::size_t maximum=1024){return !v.empty()&&v.size()<=maximum&&v.find('\0')==std::string_view::npos;}
std::string record_key(std::string_view kind,std::string_view key){return "r"+std::string(kind)+'\0'+std::string(key);}
std::string history_key(const StoredRecord& r){return "h"+r.record.kind+'\0'+r.record.key+'\0'+number(r.revision);}
std::string observation_prefix(std::string_view source,std::uint64_t epoch){return pack({std::string(source),number(epoch)});}
std::string observation_key(const ObservationClaim& c){return "o"+observation_prefix(c.source_uid,c.epoch)+number(c.sequence);}
std::string event_tuple(const EventKey& e){return pack({e.kind,e.entity,e.clock,e.producer,number(e.source_tick)});}
std::string exact_event_key(const RegistryEvent& e){return "x"+event_tuple(e.key)+pack({e.object_digest,e.origin});}
Bytes encode_record(const StoredRecord& r){return pack({r.record.kind,r.record.key,r.record.payload,pack(r.record.references),r.record.tombstone?"1":"0",number(r.revision),number(r.generation),r.digest});}
StoredRecord decode_record(std::string_view bytes){
  const auto f=fields(bytes,8);if(f.size()!=8||(f[4]!="0"&&f[4]!="1"))throw Failure(Code::corrupt,"invalid stored record");
  StoredRecord r{{f[0],f[1],f[2],fields(f[3]),f[4]=="1"},parse_number(f[5]),parse_number(f[6]),f[7]};
  if(r.revision==0||r.generation==0||r.digest!=record_digest(r.record))throw Failure(Code::corrupt,"stored record identity mismatch");
  return r;
}
Bytes encode_blob(const BlobInfo& b){return pack({b.digest,number(b.ordinal),number(b.bytes),number(b.created_generation),number(b.expired_generation),b.present?"1":"0",pack(b.references)});}
BlobInfo decode_blob(std::string_view bytes){
  const auto f=fields(bytes,7);if(f.size()!=7||(f[5]!="0"&&f[5]!="1")||!valid_digest(f[0]))throw Failure(Code::corrupt,"invalid stored blob metadata");
  BlobInfo b{f[0],parse_number(f[1]),parse_number(f[2]),parse_number(f[3]),parse_number(f[4]),f[5]=="1",fields(f[6])};
  if(b.ordinal==0||b.created_generation==0||(!b.present&&b.expired_generation==0))throw Failure(Code::corrupt,"invalid blob lifecycle metadata");
  return b;
}
Bytes encode_claim(const ObservationClaim& c){return pack({c.source_uid,number(c.epoch),number(c.sequence),c.payload_digest,c.receipt_digest});}
ObservationClaim decode_claim(std::string_view bytes){
  const auto f=fields(bytes,5);if(f.size()!=5||!text_key(f[0])||!valid_digest(f[3])||!valid_digest(f[4]))throw Failure(Code::corrupt,"invalid observation claim");
  ObservationClaim c{f[0],parse_number(f[1]),parse_number(f[2]),f[3],f[4]};if(c.sequence==0)throw Failure(Code::corrupt,"zero observation sequence");return c;
}
Bytes encode_event(const StoredEvent& e){return pack({event_tuple(e.event.key),e.event.object_digest,e.event.origin,number(e.generation),number(e.position),e.digest});}
StoredEvent decode_event(std::string_view bytes){
  const auto f=fields(bytes,6);if(f.size()!=6)throw Failure(Code::corrupt,"invalid stored event");const auto k=fields(f[0],5);if(k.size()!=5)throw Failure(Code::corrupt,"invalid event tuple");
  StoredEvent e{{{k[0],k[1],k[2],k[3],parse_number(k[4])},f[1],f[2]},parse_number(f[3]),parse_number(f[4]),f[5]};
  if(e.digest!=sha256(pack({f[0],f[1],f[2]})))throw Failure(Code::corrupt,"event identity mismatch");
  return e;
}
Bytes encode_authorizations(const std::vector<AuthorizationGuard>& authorizations){std::vector<std::string> values;for(const auto& a:authorizations)values.push_back(pack({a.principal,a.scope,number(a.credential_generation),number(a.not_after_unix_s)}));return pack(values);}
std::vector<AuthorizationGuard> decode_authorizations(std::string_view encoded){std::vector<AuthorizationGuard> result;for(const auto& raw:fields(encoded,16)){const auto f=fields(raw,4);if(f.size()!=4||!text_key(f[0],256)||!text_key(f[1],1024))throw Failure(Code::corrupt,"invalid admission authorization");result.push_back({f[0],f[1],parse_number(f[2]),parse_number(f[3])});}return result;}
Bytes encode_receipt(const CommitReceipt& r){return pack({r.operation_id,r.request_digest,r.batch_digest,r.root_digest,number(r.generation),number(r.admitted_unix_s),encode_authorizations(r.authorizations),number(static_cast<std::uint64_t>(r.commit_class))});}
CommitReceipt decode_receipt(std::string_view bytes){
  const auto f=fields(bytes,8);if((f.size()!=5&&f.size()!=7&&f.size()!=8)||!valid_digest(f[1])||!valid_digest(f[2])||!valid_digest(f[3]))throw Failure(Code::corrupt,"invalid commit receipt");CommitReceipt r{f[0],f[1],f[2],f[3],parse_number(f[4]),false};if(f.size()>=7){r.admitted_unix_s=parse_number(f[5]);r.authorizations=decode_authorizations(f[6]);}if(f.size()==8){const auto kind=parse_number(f[7]);if(kind>1)throw Failure(Code::corrupt,"invalid canonical commit class");r.commit_class=static_cast<CommitClass>(kind);}return r;
}
struct Root {
  std::string store;
  std::uint64_t generation=0,blobs=0,records=0,canonical_bytes=0,blob_bytes=0;
  Digest digest,base;
};
Bytes encode_root(const Root& r){
  std::vector<std::string> f{r.store,number(r.generation),r.digest,number(r.blobs),number(r.records),number(r.canonical_bytes),number(r.blob_bytes)};
  if(!r.base.empty())f.push_back(r.base);
  return seal(r.base.empty()?"ANAIERO-CANONICAL-ROOT/1":"ANAIERO-CANONICAL-ROOT/2",f);
}
Root decode_root(std::string_view data){
  const auto envelope=fields(data,3);if(envelope.size()!=3||(envelope[0]!="ANAIERO-CANONICAL-ROOT/1"&&envelope[0]!="ANAIERO-CANONICAL-ROOT/2"))throw Failure(Code::corrupt,"unsupported canonical root version");
  const auto f=unseal(data,envelope[0],8);if(f.size()!=(envelope[0]=="ANAIERO-CANONICAL-ROOT/1"?7U:8U))throw Failure(Code::corrupt,"invalid canonical root");
  Root r{f[0],parse_number(f[1]),parse_number(f[3]),parse_number(f[4]),parse_number(f[5]),parse_number(f[6]),f[2],f.size()==8?f[7]:Digest{}};
  if(!valid_digest(r.store)||(r.generation!=0&&!valid_digest(r.digest))||(r.generation==0&&!r.digest.empty())||(!r.base.empty()&&!valid_digest(r.base)))throw Failure(Code::corrupt,"invalid root identity");
  return r;
}
bool same_root_witness(Root a,Root b){a.base.clear();b.base.clear();return encode_root(a)==encode_root(b);}
struct BasePage {Digest digest;std::string first,last;std::uint64_t bytes=0,rows=0;};
struct CanonicalBase {
  Root anchor;
  std::string operation_id;
  Digest plan_digest,previous_base;
  std::uint64_t admitted=0,operation_epoch=0,storage_bytes=0;
  std::uint64_t summarized_records=0,expired_observations=0;
  std::uint64_t before_bytes=0,removed_transaction_bytes=0,removed_index_bytes=0;
  std::array<std::uint64_t,256> counts{};
  std::vector<BasePage> pages;
};
Bytes encode_base(const CanonicalBase& b){
  std::vector<std::string> pages,counts;for(const auto& p:b.pages)pages.push_back(pack({p.digest,p.first,p.last,number(p.bytes),number(p.rows)}));
  for(std::size_t i=0;i<b.counts.size();++i)if(b.counts[i]!=0)counts.push_back(pack({std::string(1,static_cast<char>(i)),number(b.counts[i])}));
  Root anchor=b.anchor;anchor.base.clear();
  return seal("ANAIERO-CANONICAL-BASE/1",{encode_root(anchor),b.operation_id,b.plan_digest,b.previous_base,number(b.admitted),number(b.operation_epoch),number(b.storage_bytes),number(b.summarized_records),number(b.expired_observations),number(b.before_bytes),number(b.removed_transaction_bytes),number(b.removed_index_bytes),pack(counts),pack(pages)});
}
CanonicalBase decode_base(std::string_view raw){
  const auto f=unseal(raw,"ANAIERO-CANONICAL-BASE/1",14);if(f.size()!=14)throw Failure(Code::corrupt,"invalid canonical base manifest");
  CanonicalBase b;b.anchor=decode_root(f[0]);b.operation_id=f[1];b.plan_digest=f[2];b.previous_base=f[3];b.admitted=parse_number(f[4]);b.operation_epoch=parse_number(f[5]);b.storage_bytes=parse_number(f[6]);b.summarized_records=parse_number(f[7]);b.expired_observations=parse_number(f[8]);b.before_bytes=parse_number(f[9]);b.removed_transaction_bytes=parse_number(f[10]);b.removed_index_bytes=parse_number(f[11]);
  if(!text_key(b.operation_id,256)||!valid_digest(b.plan_digest)||(!b.previous_base.empty()&&!valid_digest(b.previous_base))||!b.anchor.base.empty()||b.anchor.generation==0)throw Failure(Code::corrupt,"invalid canonical base identity");
  for(const auto& value:fields(f[12],256)){const auto c=fields(value,2);if(c.size()!=2||c[0].size()!=1||b.counts[static_cast<unsigned char>(c[0][0])]!=0)throw Failure(Code::corrupt,"invalid base row counts");b.counts[static_cast<unsigned char>(c[0][0])]=parse_number(c[1]);}
  std::string previous;std::uint64_t bytes=static_cast<std::uint64_t>(raw.size());
  for(const auto& value:fields(f[13],100000)){const auto p=fields(value,5);if(p.size()!=5||!valid_digest(p[0])||p[1].empty()||p[2]<p[1]||(!previous.empty()&&p[1]<=previous))throw Failure(Code::corrupt,"invalid ordered canonical base page");BasePage page{p[0],p[1],p[2],parse_number(p[3]),parse_number(p[4])};if(page.rows==0||page.rows>128||page.bytes>decode_limit/2)throw Failure(Code::corrupt,"invalid bounded canonical base page");previous=page.last;if(page.bytes>any_generation-bytes)throw Failure(Code::corrupt,"base size overflow");bytes+=page.bytes;b.pages.push_back(std::move(page));}
  if(bytes!=b.storage_bytes)throw Failure(Code::corrupt,"canonical base byte accounting mismatch");
  return b;
}
std::uint64_t operation_epoch(std::string_view id){
  if(!id.starts_with(operation_epoch_prefix))return 0;
  if(id.size()<=operation_logical_offset||id[operation_logical_offset-1]!='/'||id.substr(operation_logical_offset).starts_with(operation_epoch_prefix))throw Failure(Code::invalid,"invalid qualified operation identity");
  try{const auto epoch=parse_number(id.substr(operation_epoch_prefix.size(),16));if(epoch==0)throw Failure(Code::invalid,"zero qualified operation epoch");return epoch;}catch(const Failure& e){throw Failure(Code::invalid,e.what());}
}

struct Transaction {
  Root root;
  Digest predecessor;
  CommitReceipt receipt;
  std::vector<StoredRecord> records;
  std::vector<BlobInfo> blobs;
  std::vector<ObservationClaim> observations;
  std::vector<StoredEvent> events;
  std::vector<std::pair<Digest,std::uint64_t>> references;
  std::vector<RecordGuard> guards;
  std::uint64_t expected_generation=any_generation;
};
class IndexCursor {
  SealedIndex& index_;Digest root_;std::string prefix_,after_;
  std::vector<std::pair<std::string,std::string>> rows_;std::size_t offset_=0;bool more_=true;
public:
  IndexCursor(SealedIndex& index,Digest root,std::string prefix):index_(index),root_(std::move(root)),prefix_(std::move(prefix)){}
  bool next(std::pair<std::string,std::string>& row){
    if(offset_==rows_.size()){if(!more_)return false;index_.scan(root_,prefix_,after_,128,rows_,more_);offset_=0;if(rows_.empty())return false;}
    row=rows_[offset_++];after_=row.first;return true;
  }
};
Bytes encode_transaction(const Transaction& t){
  std::vector<std::string> records,blobs,claims,events,refs,guards;
  for(const auto& v:t.records)records.push_back(encode_record(v));
  for(const auto& v:t.blobs)blobs.push_back(encode_blob(v));
  for(const auto& v:t.observations)claims.push_back(encode_claim(v));
  for(const auto& v:t.events)events.push_back(encode_event(v));
  for(const auto& v:t.references)refs.push_back(pack({v.first,number(v.second)}));
  for(const auto& v:t.guards)guards.push_back(pack({v.kind,v.key,number(v.expected_revision),v.expected_digest}));
  return seal("ANAIERO-CANONICAL-TRANSACTION/4",{t.root.store,number(t.root.generation),t.predecessor,t.receipt.operation_id,t.receipt.request_digest,t.receipt.batch_digest,pack(records),pack(blobs),pack(claims),pack(events),pack(refs),number(t.root.blobs),number(t.root.records),number(t.root.canonical_bytes),number(t.root.blob_bytes),number(t.receipt.admitted_unix_s),encode_authorizations(t.receipt.authorizations),number(t.expected_generation),pack(guards),number(static_cast<std::uint64_t>(t.receipt.commit_class))});
}
Transaction decode_transaction(std::string_view raw){
  const auto envelope=fields(raw,3);if(envelope.size()!=3||(envelope[0]!="ANAIERO-CANONICAL-TRANSACTION/1"&&envelope[0]!="ANAIERO-CANONICAL-TRANSACTION/2"&&envelope[0]!="ANAIERO-CANONICAL-TRANSACTION/3"&&envelope[0]!="ANAIERO-CANONICAL-TRANSACTION/4"))throw Failure(Code::corrupt,"invalid canonical transaction schema");
  const auto f=unseal(raw,envelope[0],20);if((envelope[0]=="ANAIERO-CANONICAL-TRANSACTION/1"&&f.size()!=15)||(envelope[0]=="ANAIERO-CANONICAL-TRANSACTION/2"&&f.size()!=17)||(envelope[0]=="ANAIERO-CANONICAL-TRANSACTION/3"&&f.size()!=19)||(envelope[0]=="ANAIERO-CANONICAL-TRANSACTION/4"&&f.size()!=20))throw Failure(Code::corrupt,"invalid canonical transaction");
  Transaction t;t.root={f[0],parse_number(f[1]),parse_number(f[11]),parse_number(f[12]),parse_number(f[13]),parse_number(f[14]),sha256(raw),{}};t.predecessor=f[2];t.receipt={f[3],f[4],f[5],t.root.digest,t.root.generation,false};
  if(f.size()>=17){t.receipt.admitted_unix_s=parse_number(f[15]);t.receipt.authorizations=decode_authorizations(f[16]);for(const auto& a:t.receipt.authorizations)if(a.not_after_unix_s!=0&&t.receipt.admitted_unix_s>=a.not_after_unix_s)throw Failure(Code::corrupt,"canonical receipt was admitted after authorization expiry");}
  if(f.size()>=19){t.expected_generation=parse_number(f[17]);if(t.expected_generation!=any_generation&&t.expected_generation!=t.root.generation-1)throw Failure(Code::corrupt,"canonical generation admission fence mismatch");for(const auto& raw_guard:fields(f[18],4096)){const auto guard=fields(raw_guard,4);if(guard.size()!=4||!text_key(guard[0],128)||!text_key(guard[1])||(!guard[3].empty()&&!valid_digest(guard[3])))throw Failure(Code::corrupt,"invalid canonical record admission fence");t.guards.push_back({guard[0],guard[1],parse_number(guard[2]),guard[3]});}}
  if(f.size()==20){const auto kind=parse_number(f[19]);if(kind>1)throw Failure(Code::corrupt,"invalid canonical commit class");t.receipt.commit_class=static_cast<CommitClass>(kind);}
  if(!valid_digest(t.root.store)||!valid_digest(t.receipt.request_digest)||!valid_digest(t.receipt.batch_digest)||t.root.generation==0||(t.root.generation>1&&!valid_digest(t.predecessor)))throw Failure(Code::corrupt,"invalid transaction identity");
  for(const auto& v:fields(f[6]))t.records.push_back(decode_record(v));
  for(const auto& v:fields(f[7]))t.blobs.push_back(decode_blob(v));
  for(const auto& v:fields(f[8]))t.observations.push_back(decode_claim(v));
  for(const auto& v:fields(f[9]))t.events.push_back(decode_event(v));
  for(const auto& v:fields(f[10])){const auto pair=fields(v,2);if(pair.size()!=2||!valid_digest(pair[0]))throw Failure(Code::corrupt,"invalid reference counter");t.references.emplace_back(pair[0],parse_number(pair[1]));}
  for(const auto& r:t.records)if(r.generation!=t.root.generation)throw Failure(Code::corrupt,"record generation mismatch");
  return t;
}
void directory(const fs::path& p){
  std::error_code ec;const auto s=fs::symlink_status(p,ec);
  if(!ec&&fs::is_symlink(s))throw Failure(Code::corrupt,"storage directory symlink rejected");
  ec.clear();fs::create_directories(p,ec);if(ec||!fs::is_directory(p))throw Failure(Code::io_error,"cannot create storage directory");
}
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
  void apply_index(const Transaction&,bool preserve_admission=false);
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
  Digest build_base_index(SealedIndex&,const CanonicalBase&);
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

void NativeStore::load_base(){
  base_={};base_pages_.clear();
  if(root_.base.empty())return;
  directory(path_/"bases");directory(path_/"bases"/"pages");
  const auto raw=read_file(path_/"bases"/(root_.base+".base"),decode_limit);
  if(sha256(raw)!=root_.base)throw Failure(Code::corrupt,"canonical base digest mismatch");
  base_=decode_base(raw);
  if(base_.anchor.store!=root_.store||base_.anchor.generation>root_.generation||base_.anchor.canonical_bytes>root_.canonical_bytes)throw Failure(Code::corrupt,"canonical base scope mismatch");
  for(std::size_t i=0;i<base_.pages.size();++i)if(!base_pages_.emplace(base_.pages[i].digest,i).second)throw Failure(Code::corrupt,"repeated canonical base page");
  if(base_.anchor.generation==root_.generation&&!same_root_witness(base_.anchor,root_))throw Failure(Code::corrupt,"canonical base terminal witness mismatch");
}
std::vector<std::pair<std::string,std::string>> NativeStore::base_page(const BasePage& page){
  const auto raw=read_file(path_/"bases"/"pages"/page.digest,decode_limit/2);
  if(raw.size()!=page.bytes||sha256(raw)!=page.digest)throw Failure(Code::corrupt,"canonical base page digest mismatch");
  const auto f=unseal(raw,"ANAIERO-CANONICAL-BASE-PAGE/1",2,decode_limit/2);
  if(f.size()!=2||f[0]!=root_.store)throw Failure(Code::corrupt,"canonical base page scope mismatch");
  std::vector<std::pair<std::string,std::string>> result;std::string previous;
  for(const auto& value:fields(f[1],128)){const auto r=fields(value,2);if(r.size()!=2||r[0].empty()||(!previous.empty()&&r[0]<=previous))throw Failure(Code::corrupt,"canonical base page ordering mismatch");previous=r[0];result.emplace_back(r[0],r[1]);}
  if(result.empty()||result.size()!=page.rows||result.front().first!=page.first||result.back().first!=page.last)throw Failure(Code::corrupt,"canonical base page range mismatch");
  return result;
}
std::string NativeStore::derived_base_value(const BasePage& page,std::size_t slot,const std::pair<std::string,std::string>& row){
  if(row.first.front()=='r'||row.first.front()=='h'){const auto r=decode_record(row.second);return pack({"record/2",page.digest,number(slot),r.digest});}return row.second;
}
bool NativeStore::base_value(std::string_view key,std::string& out){
  const auto found=std::lower_bound(base_.pages.begin(),base_.pages.end(),key,[](const BasePage& page,std::string_view wanted){return page.last<wanted;});
  if(found==base_.pages.end()||key<found->first)return false;
  const auto rows=base_page(*found);const auto row=std::lower_bound(rows.begin(),rows.end(),key,[](const auto& value,std::string_view wanted){return value.first<wanted;});
  if(row==rows.end()||row->first!=key)return false;
  out=derived_base_value(*found,static_cast<std::size_t>(row-rows.begin()),*row);return true;
}
void NativeStore::initialize_index_from_base(){
  index_root_.clear();index_generation_=0;index_canonical_.clear();
  if(root_.base.empty())return;
  index_root_=build_base_index(*index_,base_);
  index_generation_=base_.anchor.generation;index_canonical_=base_.anchor.digest;
}
Digest NativeStore::build_base_index(SealedIndex& target,const CanonicalBase& base){
  std::vector<std::uint64_t> ends;ends.reserve(base.pages.size());std::uint64_t rows=0;
  for(const auto& page:base.pages){rows+=page.rows;ends.push_back(rows);}
  std::size_t loaded=base.pages.size();std::vector<std::pair<std::string,std::string>> cached;
  return target.build_sorted(rows,[&](std::uint64_t ordinal){
    const auto index=static_cast<std::size_t>(std::upper_bound(ends.begin(),ends.end(),ordinal)-ends.begin());
    if(index>=base.pages.size())throw Failure(Code::corrupt,"bulk index lookup exceeded verified base");
    const auto& page=base.pages[index];if(index!=loaded){cached=base_page(page);loaded=index;}
    const auto slot=static_cast<std::size_t>(ordinal-(index?ends[index-1]:0));const auto& row=cached.at(slot);
    return std::make_pair(row.first,derived_base_value(page,slot,row));
  });
}
void NativeStore::fill_compaction_report(CompactionReport& report,bool already)const{
  report={};report.plan_digest=base_.plan_digest;report.canonical_base_digest=root_.base;report.generation=base_.anchor.generation;report.operation_epoch=base_.operation_epoch;report.accounted_bytes_before=base_.before_bytes;report.accounted_bytes_after=usage();report.removed_transaction_bytes=base_.removed_transaction_bytes;report.removed_index_bytes=base_.removed_index_bytes;report.summarized_record_versions=base_.summarized_records;report.expired_observations=base_.expired_observations;report.activated=!root_.base.empty();report.cleanup_complete=pending_reclamation_bytes_==0;report.already_complete=already;
}
void NativeStore::cleanup_compaction(){
  const auto marker=path_/"COMPACTION";
  if(!regular_exists(marker)){pending_reclamation_bytes_=0;return;}
  const auto f=unseal(read_file(marker,16384),"ANAIERO-COMPACTION-INTENT/1",7);
  if(f.size()!=7||f[0]!=root_.store||!valid_digest(f[1])||(!f[2].empty()&&!valid_digest(f[2]))||(!f[3].empty()&&!valid_digest(f[3])))throw Failure(Code::corrupt,"invalid compaction cleanup intent");
  const auto through=parse_number(f[4]),first=parse_number(f[5]);pending_reclamation_bytes_=parse_number(f[6]);if(first==0||first>through+1)throw Failure(Code::corrupt,"invalid compaction cleanup range");
  const bool activated=!f[3].empty()&&root_.base==f[3];
  if(activated&&(base_.plan_digest!=f[1]||base_.anchor.generation!=through))throw Failure(Code::corrupt,"compaction cleanup authority mismatch");
  if(!activated&&root_.base!=f[2])throw Failure(Code::corrupt,"compaction cleanup root diverged");
  // Canonical files are removed only after ROOT names the verified replacement.
  // A prepublication interruption only removes unreferenced staging objects.
  if(activated){for(std::uint64_t g=first;g<=through;++g){remove_file(path_/"transactions"/(number(g)+".txn"));inject(FaultPoint::compaction_during_cleanup);if(g==through)break;}}
  const auto bases=path_/"bases",pages=bases/"pages";
  if(fs::exists(bases))directory(bases);
  if(fs::exists(pages))directory(pages);
  if(fs::exists(pages)){for(const auto& entry:fs::directory_iterator(pages)){const auto name=entry.path().filename().string();if(!valid_digest(name))throw Failure(Code::corrupt,"unexpected canonical base page path");if(base_pages_.count(name)==0)remove_file(entry.path());}}
  if(fs::exists(bases)){for(const auto& entry:fs::directory_iterator(bases)){if(entry.path().filename()=="pages")continue;const auto name=entry.path().filename().string();if(name.size()!=69||!name.ends_with(".base")||!valid_digest(std::string_view(name).substr(0,64)))throw Failure(Code::corrupt,"unexpected canonical base manifest path");if(name!=root_.base+".base")remove_file(entry.path());}}
  cleanup_inactive_index();remove_file(marker);pending_reclamation_bytes_=0;
}
void NativeStore::compacted_rows(bool expire_operations,const std::function<void(const std::pair<std::string,std::string>&)>& emit,std::uint64_t& summarized,std::uint64_t& expired){
  summarized=base_.summarized_records;expired=0;
  const auto each=[&](std::string_view prefix,const auto& callback){IndexCursor cursor(*index_,index_root_,std::string(prefix));std::pair<std::string,std::string> row;while(cursor.next(row))callback(row);};
  for(const auto family:{"b","e","f"})each(family,emit);
  each("h",[&](const auto& row){const auto r=indexed_record(row.second);StoredRecord latest;if(!record(r.record.kind,r.record.key,latest))throw Failure(Code::corrupt,"record history has no latest authority");if(r.revision==latest.revision){if(r.digest!=latest.digest)throw Failure(Code::corrupt,"record history/latest identity mismatch");emit({row.first,encode_record(r)});}else ++summarized;});
  const auto is_expired=[&](const ObservationClaim& c){BlobInfo payload,receipt;if(!blob(c.payload_digest,payload)||!blob(c.receipt_digest,receipt))throw Failure(Code::corrupt,"observation has no blob identity fence");return payload.expired_generation!=0&&receipt.expired_generation!=0;};
  std::string group;std::pair<std::string,std::string> boundary;bool pruning=true;
  const auto flush_boundary=[&]{if(!boundary.first.empty()){emit(boundary);boundary={};}};
  each("o",[&](const auto& row){const auto f=fields(row.second,2);if(f.size()!=2)throw Failure(Code::corrupt,"invalid observation row during compaction");const auto c=decode_claim(f[1]);const auto next=observation_prefix(c.source_uid,c.epoch);if(next!=group){flush_boundary();group=next;pruning=true;}if(pruning&&is_expired(c)){boundary=row;return;}flush_boundary();pruning=false;emit(row);});flush_boundary();
  if(!expire_operations)each("p",emit);
  // A second bounded pass emits one exact closed-through fence per stream,
  // retaining its last original claim as a boundary witness. No event is made up.
  group.clear();boundary={};pruning=true;std::uint64_t floor=0;
  const auto flush_floor=[&]{if(floor!=0){emit({"q"+group,boundary.second});if(floor>any_generation-expired)throw Failure(Code::capacity,"observation summary counter exhausted");expired+=floor;}floor=0;boundary={};};
  each("o",[&](const auto& row){const auto f=fields(row.second,2);const auto c=decode_claim(f[1]);const auto next=observation_prefix(c.source_uid,c.epoch);if(next!=group){flush_floor();group=next;pruning=true;}if(pruning&&is_expired(c)){floor=c.sequence;boundary=row;}else pruning=false;});flush_floor();
  each("r",[&](const auto& row){emit({row.first,encode_record(indexed_record(row.second))});});
  for(const auto family:{"t","v","x"})each(family,emit);
}
void NativeStore::preview_compaction_locked(const CompactionOptions& options,CompactionPlan& plan,std::uint64_t* working_files){
  require_ready();if(!writable_)throw Failure(Code::busy,"canonical admission is stopped");
  if(!text_key(options.operation_id,256)||options.operation_id.starts_with("__storage/")||options.working_budget_bytes==0)throw Failure(Code::invalid,"invalid compaction operation/budget");
  if(root_.generation==0)throw Failure(Code::invalid,"empty canonical history has no prefix to compact");
  if(options.expected_generation!=any_generation&&options.expected_generation!=root_.generation)throw Failure(Code::conflict,"compaction generation fence changed");
  if(!options.expected_root.empty()&&options.expected_root!=root_.digest)throw Failure(Code::conflict,"compaction root fence changed");
  if(options.expire_operation_receipts&&base_.operation_epoch==any_generation)throw Failure(Code::capacity,"operation epoch exhausted");
  plan={};plan.options=options;plan.options.expected_generation=root_.generation;plan.options.expected_root=root_.digest;plan.generation=root_.generation;plan.source_root=root_.digest;plan.source_base=root_.base;plan.next_operation_epoch=base_.operation_epoch+(options.expire_operation_receipts?1:0);plan.accounted_bytes_before=usage();plan.canonical_transactions_removed=root_.generation-base_.anchor.generation;
  std::uint64_t row_bytes=0,index_bytes=0,largest_node=4096,rows=0;
  compacted_rows(options.expire_operation_receipts,[&](const auto& row){++rows;const auto type=row.first.front();if(type=='r')++plan.retained_records;if(type=='b')++plan.retained_blobs;if(type=='p')++plan.retained_operations;const auto canonical=static_cast<std::uint64_t>(pack({row.first,row.second}).size()+128U);const auto indexed=static_cast<std::uint64_t>(row.first.size()+((type=='r'||type=='h')?256U:row.second.size())+512U);if(canonical>any_generation-row_bytes||indexed>any_generation-index_bytes)throw Failure(Code::capacity,"compaction size overflow");row_bytes+=canonical;index_bytes+=indexed;largest_node=std::max(largest_node,indexed);},plan.summarized_record_versions,plan.expired_observations);
  std::uint64_t height=2;for(auto n=rows;n>1;n=(n+1)/2)++height;
  const auto workspace=std::min<std::uint64_t>(rows,128U)*(height+4U)*largest_node*4U+65536U;
  if(row_bytes>any_generation-index_bytes||row_bytes+index_bytes>any_generation-workspace)throw Failure(Code::capacity,"compaction working size overflow");
  plan.required_working_bytes=row_bytes+index_bytes+workspace;
  if(rows>(any_generation-32U)/2U)throw Failure(Code::capacity,"compaction file-node estimate exhausted");
  if(working_files)*working_files=rows*2U+32U;
  plan.plan_digest=sha256(pack({"ANAIERO-COMPACTION-PLAN/1",options.operation_id,root_.store,number(root_.generation),root_.digest,root_.base,index_root_,number(options.working_budget_bytes),options.expire_operation_receipts?"1":"0",number(plan.next_operation_epoch),number(plan.required_working_bytes),number(plan.retained_records),number(plan.retained_blobs),number(plan.retained_operations),number(plan.summarized_record_versions),number(plan.expired_observations)}));
}
Result NativeStore::preview_compaction(const CompactionOptions& options,CompactionPlan& plan){std::lock_guard lock(mutex_);try{const auto stable_options=options;preview_compaction_locked(stable_options,plan);return {};}catch(const Failure& e){if(e.code==Code::corrupt){ready_=false;writable_=false;}return {e.code,e.what()};}catch(const std::exception& e){return {Code::io_error,e.what()};}}
Result NativeStore::execute_compaction(const CompactionPlan& supplied,CompactionReport& report){
  std::lock_guard lock(mutex_);bool activated=false,staged=false;report={};
  try{
    if(!root_.base.empty()&&base_.plan_digest==supplied.plan_digest){cleanup_compaction();fill_compaction_report(report,true);return {};}
    CompactionPlan plan;std::uint64_t working_files=0;preview_compaction_locked(supplied.options,plan,&working_files);
    if(!valid_digest(supplied.plan_digest)||plan.plan_digest!=supplied.plan_digest||plan.source_root!=supplied.source_root||plan.source_base!=supplied.source_base||plan.generation!=supplied.generation||plan.next_operation_epoch!=supplied.next_operation_epoch||plan.accounted_bytes_before!=supplied.accounted_bytes_before||plan.required_working_bytes!=supplied.required_working_bytes||plan.retained_records!=supplied.retained_records||plan.retained_blobs!=supplied.retained_blobs||plan.retained_operations!=supplied.retained_operations||plan.summarized_record_versions!=supplied.summarized_record_versions||plan.expired_observations!=supplied.expired_observations||plan.canonical_transactions_removed!=supplied.canonical_transactions_removed)throw Failure(Code::conflict,"compaction reviewed plan is stale or altered");
    AuditReport verified_history;audit_locked(verified_history);
    const auto used=usage();if(plan.required_working_bytes>plan.options.working_budget_bytes||used>options_.maximum_bytes||plan.required_working_bytes>options_.maximum_bytes-used)throw Failure(Code::capacity,"compaction requires declared temporary and total quota headroom");
    std::error_code space_error;const auto space=fs::space(path_,space_error);if(space_error||space.available<options_.minimum_free_bytes||plan.required_working_bytes>space.available-options_.minimum_free_bytes)throw Failure(Code::capacity,"compaction filesystem working headroom exhausted");
    // The old index remains live until its replacement is verified. Reserve
    // one replacement node and up to one bounded page per retained row, plus
    // manifest, intent and publication files. Bulk construction has no COW spill.
    require_file_capacity(path_,working_files,options_.minimum_free_file_nodes);
    directory(path_/"bases");directory(path_/"bases"/"pages");
    const auto old_index_bytes=index_->load(index_root_).bytes;
    const auto obsolete_bytes=root_.canonical_bytes-base_.anchor.canonical_bytes+base_.storage_bytes+old_index_bytes;
    const auto intent=[&](const Digest& target){return seal("ANAIERO-COMPACTION-INTENT/1",{root_.store,plan.plan_digest,root_.base,target,number(root_.generation),number(base_.anchor.generation+1),number(obsolete_bytes)});};
    write_file(path_/"COMPACTION",intent({}));staged=true;
    CanonicalBase next;next.anchor=root_;next.anchor.base.clear();next.operation_id=plan.options.operation_id;next.plan_digest=plan.plan_digest;next.previous_base=root_.base;next.admitted=last_admitted_unix_s_;next.operation_epoch=plan.next_operation_epoch;next.before_bytes=used;next.removed_transaction_bytes=root_.canonical_bytes-base_.anchor.canonical_bytes;next.removed_index_bytes=old_index_bytes;
    std::vector<std::string> rows;std::string first,last;std::size_t bytes=0;
    const auto flush=[&]{if(rows.empty())return;const auto raw=seal("ANAIERO-CANONICAL-BASE-PAGE/1",{root_.store,pack(rows)});if(raw.size()>decode_limit/2)throw Failure(Code::capacity,"canonical snapshot page exceeds decoder");const auto digest=sha256(raw);const auto destination=path_/"bases"/"pages"/digest;if(regular_exists(destination)){if(read_file(destination,decode_limit/2)!=raw)throw Failure(Code::corrupt,"canonical base CAS collision");}else write_file(destination,raw);next.pages.push_back({digest,first,last,static_cast<std::uint64_t>(raw.size()),static_cast<std::uint64_t>(rows.size())});next.storage_bytes+=raw.size();rows.clear();bytes=0;first.clear();last.clear();if(next.pages.size()>100000)throw Failure(Code::capacity,"bounded canonical base manifest exhausted");};
    compacted_rows(plan.options.expire_operation_receipts,[&](const auto& row){const auto encoded=pack({row.first,row.second});if(!rows.empty()&&(rows.size()==128||bytes+encoded.size()>8U*1024U*1024U))flush();if(first.empty())first=row.first;last=row.first;rows.push_back(encoded);bytes+=encoded.size();++next.counts[static_cast<unsigned char>(row.first.front())];},next.summarized_records,next.expired_observations);flush();
    auto base_raw=encode_base(next);next.storage_bytes+=base_raw.size();base_raw=encode_base(next);const auto digest=sha256(base_raw);write_file(path_/"bases"/(digest+".base"),base_raw);
    // Verify all snapshot bytes before the new ROOT can authorize any deletion.
    auto verified=decode_base(base_raw);if(verified.storage_bytes>plan.required_working_bytes)throw Failure(Code::capacity,"snapshot exceeded admitted working bytes");
    const auto old_directory=index_directory_;const std::string fresh=old_directory=="nodes-a"?"nodes-b":"nodes-a";const auto target=path_/"index"/fresh;
    if(fs::is_symlink(target))throw Failure(Code::corrupt,"compaction index directory symlink rejected");
    std::error_code ec;fs::remove_all(target,ec);if(ec)throw Failure(Code::io_error,"cannot clear compaction index staging");directory(target);
    auto fresh_index=std::make_unique<SealedIndex>(target);Digest fresh_root;
    fresh_root=build_base_index(*fresh_index,verified);
    if(verified.storage_bytes+fresh_index->load(fresh_root).bytes>plan.required_working_bytes)throw Failure(Code::capacity,"compaction exceeded admitted working bytes");
    write_file(path_/"COMPACTION",intent(digest));inject(FaultPoint::compaction_after_base);
    Root published=root_;published.base=digest;write_file(path_/"ROOT",encode_root(published));root_=published;base_=std::move(verified);base_pages_.clear();for(std::size_t i=0;i<base_.pages.size();++i)base_pages_.emplace(base_.pages[i].digest,i);pending_reclamation_bytes_=obsolete_bytes;activated=true;
    index_directory_=fresh;index_=std::move(fresh_index);index_root_=fresh_root;index_generation_=root_.generation;index_canonical_=root_.digest;
    inject(FaultPoint::compaction_after_root);save_index();cleanup_compaction();ready_=true;writable_=true;fill_compaction_report(report);return {};
  }catch(const Failure& e){if(staged||e.code==Code::corrupt){ready_=false;writable_=false;}if(activated)fill_compaction_report(report);return {activated?Code::committed_degraded:e.code,e.what()};}catch(const std::exception& e){if(staged){ready_=false;writable_=false;}if(activated)fill_compaction_report(report);return {activated?Code::committed_degraded:Code::io_error,e.what()};}
}
Result NativeStore::resume_compaction(CompactionReport& report){
  // Rebuilding also handles an interruption after ROOT publication but before
  // the derived coverage marker. Cleanup is conditional on the canonical base.
  const auto repaired=repair_index();if(!repaired)return repaired;
  std::lock_guard lock(mutex_);try{cleanup_compaction();fill_compaction_report(report,true);return {};}catch(const Failure& e){ready_=false;writable_=false;return {e.code,e.what()};}catch(const std::exception& e){ready_=false;writable_=false;return {Code::io_error,e.what()};}
}

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
  write_file(path_/index_coverage_name,seal("ANAIERO-INDEX-COVERAGE/3",{root_.store,index_directory_,number(index_generation_),index_canonical_,index_root_,root_.base}));
}
void NativeStore::load_index(){
  const bool current=regular_exists(path_/index_coverage_name);
  const bool legacy=!current&&legacy_index_exists(path_);
  if(!current&&!legacy){index_generation_=0;index_root_.clear();index_canonical_.clear();index_directory_="nodes-a";}
  else {
    const auto raw=read_file(path_/(current?index_coverage_name:"INDEX"),16384);const auto envelope=fields(raw,3);
    if(envelope.size()!=3||(envelope[0]!="ANAIERO-INDEX-COVERAGE/2"&&envelope[0]!="ANAIERO-INDEX-COVERAGE/3"))throw Failure(Code::corrupt,"unsupported index coverage version");
    const auto f=unseal(raw,envelope[0],6);
    if((f.size()!=5&&f.size()!=6)||f[0]!=root_.store||(f[1]!="nodes-a"&&f[1]!="nodes-b")||(f.size()==6?f[5]:Digest{})!=root_.base)throw Failure(Code::corrupt,"index store/base identity mismatch");
    index_directory_=f[1];index_generation_=parse_number(f[2]);index_canonical_=f[3];index_root_=f[4];
    if(index_generation_<base_.anchor.generation||index_generation_>root_.generation||(index_generation_==0&&(!index_root_.empty()||!index_canonical_.empty())))throw Failure(Code::corrupt,"index coverage ahead of canonical root");
    if(index_generation_!=0){const auto digest=index_generation_==base_.anchor.generation?base_.anchor.digest:transaction(index_generation_).root.digest;if(digest!=index_canonical_)throw Failure(Code::corrupt,"index coverage diverges from canonical history");}
  }
  directory(path_/"index"/index_directory_);index_=std::make_unique<SealedIndex>(path_/"index"/index_directory_);
  if(index_generation_<base_.anchor.generation)throw Failure(Code::corrupt,"index is behind the canonical base");
  (void)index_->load(index_root_);index_->finish_cleanup(index_root_);
  if(legacy){
    // Publish the validated derived marker durably before removing its old
    // spelling. A crash leaves either the original or the preferred new marker;
    // canonical ROOT/transactions and payloads are never renamed or rewritten.
    save_index();remove_file(path_/"INDEX");
  }
}
void NativeStore::recover_staging(){
  // A writer can die before renaming its first STAGING temporary. Its name
  // is fixed and owned by the native writer; it is never canonical authority.
  remove_file(path_/"STAGING.tmp");
  if(!regular_exists(path_/"STAGING"))return;
  const auto f=unseal(read_file(path_/"STAGING",decode_limit),"ANAIERO-STAGING/1");
  if(f.size()<3||f[0]!=root_.store)throw Failure(Code::corrupt,"invalid staging owner");
  const auto generation=parse_number(f[1]);
  if(generation>root_.generation){
    if(generation!=root_.generation+1||f[2]!=root_.digest)throw Failure(Code::corrupt,"staging predecessor mismatch");
    for(std::size_t i=3;i<f.size();++i){if(!valid_digest(f[i]))throw Failure(Code::corrupt,"bad staged blob identity");remove_file(path_/"blobs"/f[i]);remove_file(path_/"blobs"/(f[i]+".tmp"));}
    remove_file(path_/"transactions"/(number(generation)+".txn"));remove_file(path_/"transactions"/(number(generation)+".txn.tmp"));
    remove_file(path_/"ROOT.tmp");
  }else if(generation!=root_.generation)throw Failure(Code::corrupt,"stale staging generation");
  remove_file(path_/"STAGING");
}
void NativeStore::apply_index(const Transaction& t,bool preserve_admission){
  if(t.root.generation!=index_generation_+1||t.predecessor!=index_canonical_)throw Failure(Code::corrupt,"noncontiguous canonical/index transition");
  if(preserve_admission)index_->begin_publication();else index_->begin();
  auto next=index_root_;
  const auto put=[&](const std::string& k,const std::string& v){next=index_->put(next,k,v);};
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
  index_->publish_cleanup(next);
  index_root_=next;index_generation_=t.root.generation;index_canonical_=t.root.digest;
  save_index();index_->finish_cleanup(index_root_);
}
void NativeStore::replay_suffix(){
  while(index_generation_<root_.generation){const auto next=transaction(index_generation_+1);apply_index(next);}
  if(index_generation_!=root_.generation||index_canonical_!=root_.digest)throw Failure(Code::corrupt,"index does not cover the sealed canonical root");
}
void NativeStore::cleanup_inactive_index(){
  const auto obsolete=path_/"index"/(index_directory_=="nodes-a"?"nodes-b":"nodes-a");
  std::error_code ec;
  if(!fs::exists(obsolete,ec)){if(ec)throw Failure(Code::io_error,"cannot inspect obsolete index generation");return;}
  if(fs::is_symlink(obsolete))throw Failure(Code::corrupt,"obsolete index directory symlink rejected");
  fs::remove_all(obsolete,ec);if(ec||!os::sync_directory(path_/"index"))throw Failure(Code::io_error,"obsolete index generation cleanup failed");
}
void NativeStore::verify_canonical_history(){
  Digest previous=base_.anchor.digest;std::uint64_t bytes=base_.anchor.canonical_bytes,admitted=base_.admitted;
  for(const auto& page:base_.pages)(void)base_page(page);
  for(std::uint64_t generation=base_.anchor.generation+1;generation<=root_.generation;++generation){const auto tx=transaction(generation);if(tx.predecessor!=previous||tx.receipt.admitted_unix_s<admitted)throw Failure(Code::corrupt,"canonical history cannot be repaired by rebuilding an index");previous=tx.root.digest;admitted=tx.receipt.admitted_unix_s;bytes+=fs::file_size(path_/"transactions"/(number(generation)+".txn"));if(bytes!=tx.root.canonical_bytes)throw Failure(Code::corrupt,"canonical history byte accounting mismatch");}
  if(previous!=root_.digest||bytes!=root_.canonical_bytes)throw Failure(Code::corrupt,"canonical history does not reach the sealed root");
}
Result NativeStore::open(){
  try{
    if(options_.maximum_bytes<65536||options_.maximum_batch_bytes==0||options_.maximum_batch_bytes>decode_limit/2||options_.maximum_blob_bytes==0||options_.maximum_records==0||options_.maximum_records>1024||options_.maximum_blobs==0||options_.maximum_blobs>1024||options_.maximum_references>4096||options_.maximum_index_updates==0||options_.maximum_index_updates>1024)return {Code::invalid,"invalid native storage limits"};
    directory(path_);directory(path_/"transactions");directory(path_/"blobs");directory(path_/"index");
    (void)regular_exists(path_/"WRITER");if(!writer_.open(path_/"WRITER")||!writer_.lock_exclusive())return {Code::busy,"canonical store already has a writer"};
    if(!regular_exists(path_/"ROOT")){
      if(regular_exists(path_/index_coverage_name)||legacy_index_exists(path_)||regular_exists(path_/"STAGING")||!fs::is_empty(path_/"transactions")||!fs::is_empty(path_/"blobs")||(fs::exists(path_/"bases")&&!fs::is_empty(path_/"bases")))throw Failure(Code::corrupt,"canonical root missing from initialized storage");
      std::random_device random;root_.store=sha256(std::to_string(random())+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+path_.string());write_file(path_/"ROOT",encode_root(root_));
    }else root_=decode_root(read_file(path_/"ROOT",16384));
    generation_.store(root_.generation);load_base();last_admitted_unix_s_=base_.admitted;
    if(root_.generation>base_.anchor.generation){const auto t=transaction(root_.generation);if(!same_root_witness(t.root,root_))throw Failure(Code::corrupt,"canonical root/transaction mismatch");last_admitted_unix_s_=t.receipt.admitted_unix_s;}
    if(regular_exists(path_/"COMPACTION")){const auto pending=unseal(read_file(path_/"COMPACTION",16384),"ANAIERO-COMPACTION-INTENT/1",7);if(pending.size()!=7)throw Failure(Code::corrupt,"invalid compaction intent");pending_reclamation_bytes_=parse_number(pending[6]);}
    recover_staging();
    try{load_index();replay_suffix();cleanup_compaction();cleanup_inactive_index();ready_=true;writable_=true;}
    catch(const Failure&){ready_=false;writable_=false;} // Authoritative data is retained for explicit repair.
    return {};
  }catch(const Failure& e){return {e.code,e.what()};}catch(const std::exception& e){return {Code::io_error,e.what()};}
}
std::uint64_t NativeStore::usage()const{
  std::uint64_t index_bytes=0;
  if(index_&&!index_root_.empty()){try{index_bytes=index_->load(index_root_).bytes;}catch(...){return options_.maximum_bytes;}}
  return root_.canonical_bytes-base_.anchor.canonical_bytes+base_.storage_bytes+root_.blob_bytes+index_bytes+pending_reclamation_bytes_+16384;
}
StoreInfo NativeStore::info()const{
  std::lock_guard lock(mutex_);StoreInfo i;i.store_id=root_.store;i.generation=root_.generation;i.index_generation=index_generation_;i.blob_count=root_.blobs;i.record_count=root_.records;i.physical_bytes=usage();i.maximum_bytes=options_.maximum_bytes;i.maximum_batch_bytes=options_.maximum_batch_bytes;i.maintenance_reserve_bytes=std::min(options_.maintenance_reserve_bytes,options_.maximum_bytes/8U);i.ordinary_limit_bytes=options_.maximum_bytes-i.maintenance_reserve_bytes;i.root_digest=root_.digest;i.index_root=index_root_;i.writable=writable_;i.index_ready=ready_;i.canonical_base_generation=base_.anchor.generation;i.canonical_base_digest=root_.base;i.operation_epoch=base_.operation_epoch;i.admitted_unix_s=last_admitted_unix_s_;i.pending_reclamation_bytes=pending_reclamation_bytes_;return i;
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
    const auto reserve=static_cast<std::uint64_t>(request_bytes)*2U+static_cast<std::uint64_t>(raw.size())*2U+static_cast<std::uint64_t>(mutations)*(node_height+height_growth+2U)*largest_node*4U+32768U;
    const auto reclaimed_bytes=reclaim.empty()?0:root_.blob_bytes-tx.root.blob_bytes;
    const auto accounted=usage();if(reclaimed_bytes>accounted)throw Failure(Code::corrupt,"reclamation exceeds accounted storage bytes");const auto used=accounted-reclaimed_bytes;
    const auto maintenance_reserve=std::min(options_.maintenance_reserve_bytes,options_.maximum_bytes/8U);
    const auto budget=batch.commit_class==CommitClass::maintenance?options_.maximum_bytes:options_.maximum_bytes-maintenance_reserve;
    std::error_code space_error;const auto space=fs::space(path_,space_error);
    if(used>budget||reserve>budget-used)throw Failure(Code::capacity,batch.commit_class==CommitClass::maintenance?"finite maintenance headroom exhausted; absolute byte quota still applies":"ordinary byte budget preserves maintenance headroom; retain less or review capacity");
    if(space_error||space.available<options_.minimum_free_bytes||reserve>space.available-options_.minimum_free_bytes)throw Failure(Code::capacity,"filesystem admission headroom exhausted");
    const auto temporary_files=static_cast<std::uint64_t>(mutations)*(node_height+height_growth+2U)*4U+
      static_cast<std::uint64_t>(new_blobs.size())+16U;
    require_file_capacity(path_,temporary_files,options_.minimum_free_file_nodes);
    const auto system_seconds=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto admitted=options_.admission_clock?options_.admission_clock->unix_seconds():(system_seconds<0?0:static_cast<std::uint64_t>(system_seconds));
    if(admitted<last_admitted_unix_s_)throw Failure(Code::expired,"trusted admission clock precedes the durable time floor");
    for(const auto& a:batch.authorizations)if(a.not_after_unix_s!=0&&admitted>=a.not_after_unix_s)throw Failure(Code::expired,"authorization expired before serialized commit admission");
    tx.receipt.admitted_unix_s=admitted;raw=encode_transaction(tx);tx.root.digest=sha256(raw);tx.receipt.root_digest=tx.root.digest;
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
    measurement.phase(&CommitTimings::index_ns);apply_index(tx,true);inject(FaultPoint::after_index);
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
Result NativeStore::repair_index(){
  std::lock_guard lock(mutex_);
  try{
    // A failed rename/directory flush may have made ROOT visible before the
    // caller observed completion. Reconcile it before removing staged objects.
    root_=decode_root(read_file(path_/"ROOT",16384));generation_.store(root_.generation);load_base();last_admitted_unix_s_=base_.admitted;
    if(root_.generation>base_.anchor.generation){const auto t=transaction(root_.generation);if(!same_root_witness(t.root,root_))throw Failure(Code::corrupt,"repair root witness mismatch");last_admitted_unix_s_=t.receipt.admitted_unix_s;}
    recover_staging();
    verify_canonical_history();
    bool suffix=false;
    try{load_index();index_->audit(index_root_);suffix=true;}catch(const Failure&){suffix=false;}
    if(suffix){try{replay_suffix();cleanup_compaction();cleanup_inactive_index();ready_=true;writable_=true;return {};}catch(const Failure&){suffix=false;}}
    (void)suffix;
    const auto old_directory=index_directory_;const auto fresh=old_directory=="nodes-a"?"nodes-b":"nodes-a";
    const auto target=path_/"index"/fresh;
    if(fs::is_symlink(target))throw Failure(Code::corrupt,"rebuild directory symlink rejected");
    std::error_code ec;fs::remove_all(target,ec);if(ec)throw Failure(Code::io_error,"cannot clear inactive index generation");directory(target);
    index_directory_=fresh;index_=std::make_unique<SealedIndex>(target);index_root_.clear();index_generation_=0;index_canonical_.clear();
    // Coverage is advanced after each sealed transaction. A rebuild interrupted
    // after any published prefix resumes through the same canonical suffix.
    initialize_index_from_base();save_index();replay_suffix();cleanup_compaction();ready_=true;writable_=true;
    const auto old=path_/"index"/old_directory;if(old!=target){fs::remove_all(old,ec);if(ec)throw Failure(Code::io_error,"obsolete index generation cleanup failed");}
    return {};
  }catch(const Failure& e){ready_=false;writable_=false;return {e.code,e.what()};}catch(const std::exception& e){ready_=false;writable_=false;return {Code::io_error,e.what()};}
}
void NativeStore::audit_compacted(AuditReport& report){
  verify_canonical_history();report={};report.canonical_base_digest=root_.base;report.base_pages=base_.pages.size();report.summarized_generations=base_.anchor.generation;report.summarized_record_versions=base_.summarized_records;report.expired_observations=base_.expired_observations;report.records=base_.counts['h'];
  std::array<std::uint64_t,256> base_counts{};
  for(const auto& page:base_.pages)for(const auto& row:base_page(page)){++base_counts[static_cast<unsigned char>(row.first.front())];if(row.first.front()=='r'||row.first.front()=='h'){const auto r=decode_record(row.second);if(r.generation>base_.anchor.generation||row.first!=(row.first.front()=='r'?record_key(r.record.kind,r.record.key):history_key(r)))throw Failure(Code::corrupt,"canonical base record witness mismatch");++report.canonical_records_read;}}
  if(base_counts!=base_.counts||base_counts['r']!=base_.anchor.records||base_counts['h']!=base_counts['r']||base_counts['b']!=base_.anchor.blobs||base_counts['v']!=base_.anchor.blobs)throw Failure(Code::corrupt,"canonical base coverage mismatch");
  std::uint64_t claims=base_.counts['o'],events=base_.counts['x'],tips=base_.counts['t'],operations=base_.counts['p'];
  for(std::uint64_t g=base_.anchor.generation+1;g<=root_.generation;++g){const auto t=transaction(g);++report.generations;++operations;report.records+=t.records.size();report.canonical_records_read+=t.records.size()+t.observations.size()+t.events.size();claims+=t.observations.size();events+=t.events.size();for(const auto& c:t.observations)if(c.sequence==1)++tips;}
  const auto before=index_->stats.nodes_read;index_->audit(index_root_);
  std::array<std::uint64_t,256> counts{};std::array<std::uint64_t,5> expected_references{},indexed_references{};
  const auto fingerprint=[](auto& aggregate,std::string_view digest,std::uint64_t count){if(!valid_digest(digest))throw Failure(Code::corrupt,"invalid reference fingerprint identity");aggregate[0]+=count;for(std::size_t i=0;i<4;++i)aggregate[i+1]+=parse_number(digest.substr(i*16,16))*count;};
  std::size_t page_number=0;std::vector<std::pair<std::string,std::string>> cached_rows;std::size_t cached_number=base_.pages.size();
  const auto baseline=[&](const auto& row){while(page_number<base_.pages.size()&&base_.pages[page_number].last<row.first)++page_number;if(page_number==base_.pages.size()||row.first<base_.pages[page_number].first)return false;if(cached_number!=page_number){cached_rows=base_page(base_.pages[page_number]);cached_number=page_number;}const auto found=std::lower_bound(cached_rows.begin(),cached_rows.end(),row.first,[](const auto& item,const auto& wanted){return item.first<wanted;});return found!=cached_rows.end()&&found->first==row.first&&derived_base_value(base_.pages[page_number],static_cast<std::size_t>(found-cached_rows.begin()),*found)==row.second;};
  IndexCursor all(*index_,index_root_,"");std::pair<std::string,std::string> row;
  while(all.next(row)){
    const auto type=row.first.front();++counts[static_cast<unsigned char>(type)];const bool from_base=baseline(row);
    if(type=='r'||type=='h'){const auto r=indexed_record(row.second);if(row.first!=(type=='r'?record_key(r.record.kind,r.record.key):history_key(r)))throw Failure(Code::corrupt,"record index has no canonical key");if(!from_base&&r.generation<=base_.anchor.generation)throw Failure(Code::corrupt,"record index differs from canonical base");if(type=='r'&&!r.record.tombstone)for(const auto& ref:r.record.references)fingerprint(expected_references,ref,1);}
    else if(type=='p'){const auto receipt=decode_receipt(row.second);if(row.first!="p"+receipt.operation_id)throw Failure(Code::corrupt,"operation index key mismatch");if(!from_base){if(receipt.generation<=base_.anchor.generation||encode_receipt(transaction(receipt.generation).receipt)!=row.second)throw Failure(Code::corrupt,"operation index lacks retained canonical witness");}}
    else if(type=='o'||type=='t'||type=='q'){const auto f=fields(row.second,2);if(f.size()!=2)throw Failure(Code::corrupt,"bad observation/floor index");const auto c=decode_claim(f[1]);if(row.first!=(type=='o'?observation_key(c):std::string(1,type)+observation_prefix(c.source_uid,c.epoch)))throw Failure(Code::corrupt,"observation/floor key mismatch");if(type=='q'&&!from_base)throw Failure(Code::corrupt,"unwitnessed observation expiry floor");if(!from_base){const auto generation=parse_number(f[0]);if(generation<=base_.anchor.generation)throw Failure(Code::corrupt,"observation index differs from canonical base");const auto t=transaction(generation);bool match=false;for(const auto& claim:t.observations)if(encode_claim(claim)==f[1])match=true;if(!match)throw Failure(Code::corrupt,"observation index lacks retained canonical witness");}}
    else if(type=='e'||type=='x'){const auto e=decode_event(row.second);if(row.first!=(type=='e'?"e"+event_tuple(e.event.key):exact_event_key(e.event)))throw Failure(Code::corrupt,"event index key mismatch");if(!from_base){if(e.generation<=base_.anchor.generation)throw Failure(Code::corrupt,"event index differs from canonical base");const auto t=transaction(e.generation);if(e.position>=t.events.size()||encode_event(t.events[static_cast<std::size_t>(e.position)])!=row.second)throw Failure(Code::corrupt,"event index lacks retained canonical witness");}}
    else if(type=='b'){
      const auto b=decode_blob(row.second);if(row.first!="b"+b.digest)throw Failure(Code::corrupt,"blob index key mismatch");++report.blobs;
      if(!from_base){
        if(b.created_generation<=base_.anchor.generation){std::string raw;if(!base_value(row.first,raw))throw Failure(Code::corrupt,"blob has no canonical base creation witness");const auto original=decode_blob(raw);if(original.ordinal!=b.ordinal||original.bytes!=b.bytes||original.references!=b.references||original.created_generation!=b.created_generation||(original.expired_generation!=0&&original.expired_generation!=b.expired_generation)||(!original.present&&b.present))throw Failure(Code::corrupt,"blob lifecycle differs from canonical base");}
        else {const auto creation=transaction(b.created_generation);bool found=false;for(const auto& candidate:creation.blobs)if(candidate.digest==b.digest&&candidate.ordinal==b.ordinal&&candidate.bytes==b.bytes&&candidate.references==b.references&&candidate.expired_generation==0&&candidate.present)found=true;if(!found)throw Failure(Code::corrupt,"blob lacks canonical creation witness");}
        if(b.expired_generation>base_.anchor.generation){const auto expiry=transaction(b.expired_generation);bool found=false;for(const auto& candidate:expiry.blobs)if(candidate.digest==b.digest&&candidate.expired_generation==b.expired_generation)found=true;if(!found)throw Failure(Code::corrupt,"blob lacks canonical expiry witness");}
      }
      if(b.expired_generation!=0){++report.expired_blobs;continue;}
      for(const auto& ref:b.references)fingerprint(expected_references,ref,1);
      const auto raw=read_file(path_/"blobs"/b.digest,options_.maximum_blob_bytes);if(raw.size()!=b.bytes||sha256(raw)!=b.digest)throw Failure(Code::corrupt,"retained blob digest mismatch");
    }
    else if(type=='f'){const auto f=fields(row.second,2);if(f.size()!=2)throw Failure(Code::corrupt,"bad reference index");if(!from_base){const auto generation=parse_number(f[0]);if(generation<=base_.anchor.generation)throw Failure(Code::corrupt,"reference differs from canonical base");const auto t=transaction(generation);bool found=false;for(const auto& ref:t.references)if(row.first=="f"+ref.first&&f[1]==number(ref.second))found=true;if(!found)throw Failure(Code::corrupt,"reference lacks canonical witness");}fingerprint(indexed_references,std::string_view(row.first).substr(1),parse_number(f[1]));}
    else if(type=='v'){BlobInfo b;if(!blob(row.second,b)||row.first!="v"+number(b.ordinal))throw Failure(Code::corrupt,"blob inventory mismatch");}
    else throw Failure(Code::corrupt,"unknown compacted index entry type");
  }
  if(counts['r']!=root_.records||counts['h']!=report.records||counts['p']!=operations||counts['o']!=claims||counts['t']!=tips||counts['q']!=base_.counts['q']||counts['x']!=events||counts['b']!=root_.blobs||counts['v']!=root_.blobs||expected_references!=indexed_references)throw Failure(Code::corrupt,"compacted index coverage/reference mismatch");
  for(const auto& family:std::array<std::pair<std::string,std::string>,3>{{{"h","r"},{"o","t"},{"x","e"}}}){
    IndexCursor history(*index_,index_root_,family.first),current(*index_,index_root_,family.second);std::pair<std::string,std::string> item,actual;std::string group,latest_value;std::uint64_t revision=0,generation=0,position=0;
    const auto finish=[&]{if(group.empty())return;if(!current.next(actual)||actual.first!=group)throw Failure(Code::corrupt,"current index differs from compacted history");if(family.first=="h"){const auto a=indexed_record(actual.second),b=indexed_record(latest_value);if(a.digest!=b.digest||a.generation!=b.generation||a.revision!=b.revision)throw Failure(Code::corrupt,"latest record differs from compacted history");}else if(actual.second!=latest_value)throw Failure(Code::corrupt,"latest source/event differs from compacted history");};
    while(history.next(item)){
      std::string key;std::uint64_t next_revision=0,next_generation=0,next_position=0;
      if(family.first=="h"){const auto r=indexed_record(item.second);key=record_key(r.record.kind,r.record.key);next_revision=r.revision;next_generation=r.generation;}
      else if(family.first=="o"){const auto f=fields(item.second,2);const auto c=decode_claim(f[1]);key="t"+observation_prefix(c.source_uid,c.epoch);next_revision=c.sequence;next_generation=parse_number(f[0]);}
      else {const auto e=decode_event(item.second);key="e"+event_tuple(e.event.key);next_generation=e.generation;next_position=e.position;}
      if(key!=group){finish();group=key;latest_value.clear();revision=0;generation=0;position=0;if(family.first=="h"){std::string original;if(base_value(key,original))revision=indexed_record(original).revision-1;}else if(family.first=="o"){bool found=false;const auto floor=value("q"+key.substr(1),found);if(found){const auto f=fields(floor,2);revision=decode_claim(f[1]).sequence-1;}}}
      if(family.first!="x"){if(next_revision!=revision+1||next_generation<generation||(family.first=="h"&&next_generation==generation))throw Failure(Code::corrupt,"compacted history has a gap/reversal");revision=next_revision;generation=next_generation;latest_value=item.second;}
      else if(latest_value.empty()||next_generation>generation||(next_generation==generation&&next_position>position)){generation=next_generation;position=next_position;latest_value=item.second;}
    }
    finish();if(current.next(actual))throw Failure(Code::corrupt,"current index has an unwitnessed compacted identity");
  }
  report.index_nodes_read=index_->stats.nodes_read-before;
}

void NativeStore::audit_locked(AuditReport& report){
  if(!root_.base.empty()){audit_compacted(report);return;}
  report={};Digest previous;std::uint64_t canonical_bytes=0,claims=0,events=0,tips=0,admitted=0;
  for(std::uint64_t g=1;g<=root_.generation;++g){const auto t=transaction(g);++report.generations;report.records+=t.records.size();report.canonical_records_read+=t.records.size()+t.observations.size()+t.events.size();claims+=t.observations.size();events+=t.events.size();for(const auto& c:t.observations)if(c.sequence==1)++tips;if(t.predecessor!=previous||t.receipt.admitted_unix_s<admitted)throw Failure(Code::corrupt,"historical canonical predecessor/admission order mismatch");admitted=t.receipt.admitted_unix_s;previous=t.root.digest;canonical_bytes+=fs::file_size(path_/"transactions"/(number(g)+".txn"));if(t.root.canonical_bytes!=canonical_bytes)throw Failure(Code::corrupt,"canonical byte accounting mismatch");}
  if(previous!=root_.digest||canonical_bytes!=root_.canonical_bytes)throw Failure(Code::corrupt,"canonical audit root mismatch");
  const auto before=index_->stats.nodes_read;index_->audit(index_root_);
  std::array<std::uint64_t,256> counts{};
  std::array<std::uint64_t,5> expected_references{},indexed_references{};
  const auto reference_fingerprint=[](std::array<std::uint64_t,5>& aggregate,std::string_view digest,std::uint64_t count){if(!valid_digest(digest))throw Failure(Code::corrupt,"invalid reference fingerprint identity");aggregate[0]+=count;for(std::size_t i=0;i<4;++i)aggregate[i+1]+=parse_number(digest.substr(i*16,16))*count;};
  std::vector<std::pair<std::string,std::string>> rows;bool more=true;std::string after;
  // Each index entry points to a bounded canonical transaction. This validates
  // historical and latest indexes without scanning history per observation.
  while(more){index_->scan(index_root_,"",after,128,rows,more);for(const auto& row:rows){after=row.first;const auto type=row.first.front();++counts[static_cast<unsigned char>(type)];
    if(type=='r'||type=='h'){const auto r=indexed_record(row.second);if(row.first!=(type=='r'?record_key(r.record.kind,r.record.key):history_key(r)))throw Failure(Code::corrupt,"record index has no matching canonical key");if(type=='r'&&!r.record.tombstone)for(const auto& ref:r.record.references)reference_fingerprint(expected_references,ref,1);}
    else if(type=='p'){const auto receipt=decode_receipt(row.second);const auto t=transaction(receipt.generation);if(encode_receipt(t.receipt)!=encode_receipt(receipt)||row.first!="p"+receipt.operation_id)throw Failure(Code::corrupt,"operation index has no canonical witness");}
    else if(type=='o'||type=='t'){const auto f=fields(row.second,2);if(f.size()!=2)throw Failure(Code::corrupt,"bad observation index");const auto claim=decode_claim(f[1]);if(row.first!=(type=='o'?observation_key(claim):"t"+observation_prefix(claim.source_uid,claim.epoch)))throw Failure(Code::corrupt,"observation index key mismatch");const auto t=transaction(parse_number(f[0]));bool match=false;for(const auto& c:t.observations)if(encode_claim(c)==f[1]){match=true;break;}if(!match)throw Failure(Code::corrupt,"observation index has no canonical witness");}
    else if(type=='e'||type=='x'){const auto e=decode_event(row.second);const auto t=transaction(e.generation);if(row.first!=(type=='e'?"e"+event_tuple(e.event.key):exact_event_key(e.event))||e.position>=t.events.size()||encode_event(t.events[static_cast<std::size_t>(e.position)])!=row.second)throw Failure(Code::corrupt,"event index has no canonical witness");}
    else if(type=='b'){
      const auto b=decode_blob(row.second);if(row.first!="b"+b.digest)throw Failure(Code::corrupt,"blob index key mismatch");++report.blobs;
      const auto creation=transaction(b.created_generation);bool created=false;for(const auto& candidate:creation.blobs)if(candidate.digest==b.digest&&candidate.ordinal==b.ordinal&&candidate.bytes==b.bytes&&candidate.references==b.references&&candidate.expired_generation==0&&candidate.present){created=true;break;}if(!created)throw Failure(Code::corrupt,"blob has no canonical creation witness");
      if(b.expired_generation!=0){++report.expired_blobs;const auto expiry=transaction(b.expired_generation);bool expired=false;for(const auto& candidate:expiry.blobs)if(candidate.digest==b.digest&&candidate.expired_generation==b.expired_generation){expired=true;break;}if(!expired)throw Failure(Code::corrupt,"blob has no canonical expiry witness");continue;}
      for(const auto& ref:b.references)reference_fingerprint(expected_references,ref,1);
      const auto raw=read_file(path_/"blobs"/b.digest,options_.maximum_blob_bytes);if(raw.size()!=b.bytes||sha256(raw)!=b.digest)throw Failure(Code::corrupt,"historical audit blob digest mismatch");
    }
    else if(type=='f'){const auto f=fields(row.second,2);if(f.size()!=2)throw Failure(Code::corrupt,"bad reference index");const auto t=transaction(parse_number(f[0]));bool match=false;for(const auto& ref:t.references)if(row.first=="f"+ref.first&&f[1]==number(ref.second)){match=true;break;}if(!match)throw Failure(Code::corrupt,"reference index has no canonical witness");reference_fingerprint(indexed_references,std::string_view(row.first).substr(1),parse_number(f[1]));}
    else if(type=='v'){BlobInfo b;if(!blob(row.second,b)||row.first!="v"+number(b.ordinal))throw Failure(Code::corrupt,"blob inventory index mismatch");}
    else throw Failure(Code::corrupt,"unknown index entry type");
  }}
  if(counts['r']!=root_.records||counts['h']!=report.records||counts['p']!=root_.generation||counts['o']!=claims||counts['t']!=tips||counts['x']!=events||counts['b']!=root_.blobs||counts['v']!=root_.blobs||expected_references!=indexed_references)throw Failure(Code::corrupt,"index coverage/reference aggregate mismatch");
  // Merge the ordered immutable occurrence histories with current directories.
  // Only two fixed-size pages and one group accumulator are resident, even for
  // fully retired identities. No audit observation triggers a historical scan.
  for(const auto& family:std::array<std::pair<std::string,std::string>,3>{{{"h","r"},{"o","t"},{"x","e"}}}){
    IndexCursor history(*index_,index_root_,family.first),current(*index_,index_root_,family.second);std::pair<std::string,std::string> row,actual;std::string group,latest_value;std::uint64_t revision=0,generation=0,position=0;
    const auto finish=[&]{if(group.empty())return;if(!current.next(actual)||actual.first!=group||actual.second!=latest_value)throw Failure(Code::corrupt,"current index differs from ordered canonical history");};
    while(history.next(row)){
      std::string key;std::uint64_t next_revision=0,next_generation=0,next_position=0;
      if(family.first=="h"){const auto r=indexed_record(row.second);key=record_key(r.record.kind,r.record.key);next_revision=r.revision;next_generation=r.generation;}
      else if(family.first=="o"){const auto f=fields(row.second,2);const auto c=decode_claim(f[1]);key="t"+observation_prefix(c.source_uid,c.epoch);next_revision=c.sequence;next_generation=parse_number(f[0]);}
      else {const auto e=decode_event(row.second);key="e"+event_tuple(e.event.key);next_generation=e.generation;next_position=e.position;}
      if(key!=group){finish();group=key;latest_value.clear();revision=0;generation=0;position=0;}
      if(family.first!="x"){if(next_revision!=revision+1||next_generation<generation||(family.first=="h"&&next_generation==generation))throw Failure(Code::corrupt,"record/source history revision gap or reversal");revision=next_revision;generation=next_generation;latest_value=row.second;}
      else if(latest_value.empty()||next_generation>generation||(next_generation==generation&&next_position>position)){generation=next_generation;position=next_position;latest_value=row.second;}
    }
    finish();if(current.next(actual))throw Failure(Code::corrupt,"current index contains an unwitnessed identity");
  }
  report.index_nodes_read=index_->stats.nodes_read-before;
}
Result NativeStore::audit(AuditReport& report){return read([&]{audit_locked(report);});}
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

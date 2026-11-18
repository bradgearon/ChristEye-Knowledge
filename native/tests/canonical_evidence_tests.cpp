#include "anaiero/sdk/evidence.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace anaiero::sdk;
using namespace anaiero::sdk::evidence;
namespace fs=std::filesystem;
namespace {
void expect(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
void require(Result result,const std::string& message){if(!result)throw std::runtime_error(message+": "+std::string(code_name(result.code))+" "+result.message);}
struct Temporary {
  fs::path path;
  explicit Temporary(const std::string& label){path=fs::temp_directory_path()/("anaiero-canonical-"+label+'-'+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(path);}
  ~Temporary(){std::error_code ec;fs::remove_all(path,ec);}
};
std::unique_ptr<ICommitStore> open(const fs::path& path,const StoreOptions& options={}){std::unique_ptr<ICommitStore> store;require(open_native_store(path.string(),options,store),"open");return store;}
CommitBatch batch(const std::string& id){CommitBatch b;b.operation_id=id;b.request_digest=sha256(id);return b;}
Blob object(std::string bytes,std::vector<Digest> references={}){const auto digest=sha256(bytes);return {digest,std::move(bytes),std::move(references)};}
CommitReceipt commit(ICommitStore& store,const CommitBatch& b){CommitReceipt r;require(store.commit(b,r),"commit "+b.operation_id);return r;}
void overwrite(const fs::path& path,std::string_view bytes){std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));expect(static_cast<bool>(out),"test overwrite");}
std::string fixture_bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);expect(static_cast<bool>(in),"open fixture bytes");return {std::istreambuf_iterator<char>(in),{}};}
void coverage_marker_paths(){
  Temporary temp("coverage-paths");auto store=open(temp.path);expect(store->info().index_ready&&store->info().writable,"fresh storage opens on case-insensitive filesystems");
  auto b=batch("coverage-path-operation");const auto payload=object("preserved canonical payload");b.blobs={payload};b.records={{"coverage","state","preserved canonical record",{payload.digest},false}};const auto receipt=commit(*store,b);
  const auto marker=temp.path/"INDEX-COVERAGE",legacy=temp.path/"INDEX";
  expect(fs::is_regular_file(marker)&&fs::is_directory(temp.path/"index")&&!fs::equivalent(marker,temp.path/"index"),"coverage marker and index directory have distinct filesystem identities");
  const auto original_root=fixture_bytes(temp.path/"ROOT"),original_tx=fixture_bytes(temp.path/"transactions"/"0000000000000001.txn"),coverage=fixture_bytes(marker);
  store.reset();store=open(temp.path);expect(store->info().index_ready,"new coverage marker reopens");
  std::error_code ec;const bool alias=fs::equivalent(legacy,temp.path/"index",ec);
  if(ec||!alias){
    // Only case-sensitive filesystems could contain the original two names.
    store.reset();fs::rename(marker,legacy);store=open(temp.path);
    expect(store->info().index_ready&&fs::is_regular_file(marker)&&!fs::exists(legacy),"validated legacy coverage is adopted without a historical rebuild");
    expect(native_index_statistics(*store).nodes_read<8,"legacy marker adoption preserves bounded healthy startup");
    CommitReceipt retried;require(store->lookup_operation(b.operation_id,retried),"legacy operation lookup");expect(retried.root_digest==receipt.root_digest,"legacy adoption preserves original receipts");
    expect(fixture_bytes(temp.path/"ROOT")==original_root&&fixture_bytes(temp.path/"transactions"/"0000000000000001.txn")==original_tx&&fixture_bytes(temp.path/"blobs"/payload.digest)==payload.bytes,"legacy adoption does not rewrite canonical evidence");

    store.reset();overwrite(legacy,coverage);overwrite(marker,"damaged current coverage");store=open(temp.path);
    expect(!store->info().index_ready&&!store->info().writable,"damaged current coverage cannot fall back to a legacy marker");
    expect(fixture_bytes(legacy)==coverage,"failed coverage loading preserves the older marker");require(store->repair_index(),"explicit repair of preferred marker");
    store.reset();overwrite(legacy,"unowned old-name bytes");store=open(temp.path);
    expect(store->info().index_ready&&fixture_bytes(legacy)=="unowned old-name bytes","preferred marker does not overwrite unrelated old-name bytes");
    store.reset();fs::remove(marker);store=open(temp.path);
    expect(!store->info().index_ready&&!fs::exists(marker)&&fixture_bytes(legacy)=="unowned old-name bytes","invalid legacy coverage is preserved and fails closed");
    require(store->repair_index(),"rebuild with invalid legacy marker retained");expect(fixture_bytes(legacy)=="unowned old-name bytes","repair does not delete invalid legacy marker");
  }
  StoredRecord state;require(store->latest("coverage","state",state),"coverage record remains readable");expect(state.record.payload==b.records.front().payload,"marker compatibility preserves record bytes");AuditReport report;require(store->audit(report),"coverage compatibility audit");
  store.reset();fs::remove(marker);fs::create_directory(marker);overwrite(marker/"keep","unowned directory contents");store=open(temp.path);
  expect(!store->info().index_ready&&!store->info().writable,"nonregular current coverage fails closed");expect(store->repair_index().code==Code::corrupt&&fixture_bytes(marker/"keep")=="unowned directory contents","repair preserves nonregular coverage paths");
#ifndef _WIN32
  store.reset();fs::remove_all(marker);const auto sentinel=temp.path/"sentinel";overwrite(sentinel,"unowned symlink target");fs::create_symlink(sentinel,marker);store=open(temp.path);
  expect(!store->info().index_ready&&store->repair_index().code==Code::corrupt&&fixture_bytes(sentinel)=="unowned symlink target","coverage symlink is rejected without changing its target");
#endif
}
void atomic_transitions(){
  Temporary temp("atomic");auto store=open(temp.path);auto b=batch("transition-1");const auto config=object("configuration");const auto checkpoint=object("checkpoint");const auto observation=object("observation");const auto receipt=object("transition receipt");
  b.blobs={config,checkpoint,observation,receipt};b.records={{"engine","cell","state-1",{config.digest,checkpoint.digest},false},{"policy","source","revision-1",{},false},{"lifecycle","active","source-a/1",{},false}};b.guards={{"engine","cell",0,{}},{"policy","source",0,{}}};b.observations={{"source-a",1,1,observation.digest,receipt.digest}};
  const auto r=commit(*store,b);expect(r.generation==1&&r.root_digest==store->info().root_digest,"one canonical root for full transition");StoredRecord state;require(store->latest("engine","cell",state),"read engine");expect(state.record.payload=="state-1"&&state.generation==r.generation&&state.revision==1,"coherent engine state");ObservationClaim c;require(store->lookup_observation("source-a",1,1,c),"strict observation lookup");expect(c.receipt_digest==receipt.digest,"receipt belongs to observation");
  std::unique_ptr<ICommitStore> competing;expect(open_native_store(temp.path.string(),{},competing).code==Code::busy,"single writer lock");
  b.expected_generation=99;b.guards[0].expected_revision=99;CommitReceipt duplicate;require(store->commit(b,duplicate),"retry ignores newly stale admission preconditions");expect(duplicate.duplicate&&duplicate.generation==1,"same operation returns original receipt");
  b.records[0].payload="changed";expect(store->commit(b,duplicate).code==Code::conflict,"operation ID conflicts with changed effects");
  auto revoke=batch("revoke");revoke.guards={{"policy","source",1,{}}};revoke.records={{"policy","source","revoked",{},false}};commit(*store,revoke);
  auto delayed=batch("late-authorized-work");delayed.guards={{"policy","source",1,{}}};delayed.records={{"engine","cell","unauthorized effect",{},false}};expect(store->commit(delayed,duplicate).code==Code::conflict,"policy revision fence orders revocation before late work");
  store.reset();store=open(temp.path);require(store->latest("engine","cell",state),"restart checkpoint");expect(state.record.payload=="state-1","rejected work has no state");AuditReport report;require(store->audit(report),"full transition audit");expect(report.generations==2&&report.blobs==4,"audit counts");
}
void crash_matrix(){
  const FaultPoint cuts[]{FaultPoint::after_objects,FaultPoint::after_transaction,FaultPoint::after_root,FaultPoint::before_index,FaultPoint::after_index};
  for(const auto cut:cuts){Temporary temp("cut"+std::to_string(static_cast<unsigned>(cut)));auto store=open(temp.path);auto b=batch("cut-operation");const auto a=object("cut-checkpoint");b.blobs={a};b.records={{"engine","one","committed",{a.digest},false}};set_native_store_fault(*store,cut);CommitReceipt receipt;const auto result=store->commit(b,receipt);const bool sealed=cut==FaultPoint::after_root||cut==FaultPoint::before_index||cut==FaultPoint::after_index;expect(result.code==(sealed?Code::committed_degraded:Code::io_error),"crash cut distinguishes committed from pre-root failure");store.reset();store=open(temp.path);expect(store->info().index_ready&&store->info().writable,"restart restores bounded suffix/index");StoredRecord r;const auto loaded=store->latest("engine","one",r);expect(sealed?loaded.ok():loaded.code==Code::not_found,"all-or-none state at crash boundary");expect(fs::exists(temp.path/"blobs"/a.digest)==sealed,"uncommitted object orphan removed");const auto retry=commit(*store,b);expect(retry.generation==1&&retry.duplicate==sealed,"cut retry commits once");AuditReport report;require(store->audit(report),"cut audit");}
}
void strict_and_generic_history(){
  Temporary temp("semantics");auto store=open(temp.path);const auto a=object("a"),b=object("b"),r=object("receipt");const EventKey key{"observation","entity","clock","producer",7};auto first=batch("first");first.blobs={a,b,r};first.events={{key,a.digest,"origin-one"},{key,b.digest,"origin-two"}};first.observations={{"source",1,1,a.digest,r.digest}};commit(*store,first);
  StoredEvent event;require(store->find_event(key,event),"latest tuple");expect(event.event.object_digest==b.digest,"different object/origin remains distinct occurrence");require(store->find_event(key,event,a.digest,"origin-one"),"old exact occurrence");expect(event.generation==1,"old occurrence remains indexed");
  auto duplicate=batch("duplicate-occurrence");duplicate.events={{key,a.digest,"origin-one"}};duplicate.observations=first.observations;commit(*store,duplicate);require(store->find_event(key,event),"latest after duplicate");expect(event.event.object_digest==b.digest,"exact duplicate does not become latest again");
  auto conflict=batch("conflicting-sequence");conflict.observations={{"source",1,1,b.digest,r.digest}};CommitReceipt receipt;expect(store->commit(conflict,receipt).code==Code::conflict,"different strict observation rejected");conflict.operation_id="out-of-order";conflict.observations[0].sequence=3;expect(store->commit(conflict,receipt).code==Code::conflict,"sequence gap rejected");conflict.operation_id="in-order";conflict.observations[0].sequence=2;commit(*store,conflict);
  auto expired=batch("expire-history");expired.expire_blobs={a.digest};commit(*store,expired);Bytes bytes;expect(store->get_blob(a.digest,bytes).code==Code::history_expired,"declared history expiry");ObservationClaim c;require(store->lookup_observation("source",1,1,c),"expired payload retains observation identity");require(store->reclaim_expired_blob(a.digest,store->generation()),"physical historical reclaim");require(store->find_event(key,event,a.digest,"origin-one"),"retained generic occurrence after reclaim");AuditReport report;require(store->audit(report),"expired evidence audit");expect(report.expired_blobs==1,"audit distinguishes unavailable bytes");
}
void coalesced_source_tips(){
  Temporary temp("batched-tips");auto store=open(temp.path);const auto payload=object("shared observation"),receipt=object("batched receipt");
  auto first=batch("batched-source-claims");first.blobs={payload,receipt};
  for(std::uint64_t sequence=1;sequence<=24;++sequence){first.observations.push_back({"source-a",1,sequence,payload.digest,receipt.digest});first.observations.push_back({"source-b",2,sequence,payload.digest,receipt.digest});}
  set_native_store_fault(*store,FaultPoint::before_index);CommitReceipt committed;
  expect(store->commit(first,committed).code==Code::committed_degraded,"batched tips are canonical before derived publication");
  store.reset();store=open(temp.path);expect(store->info().index_ready,"restart replays every claim and the final tip of each source");
  for(std::uint64_t sequence=1;sequence<=24;++sequence){ObservationClaim claim;require(store->lookup_observation("source-a",1,sequence,claim),"source-a batch claim");expect(claim.payload_digest==payload.digest&&claim.receipt_digest==receipt.digest,"coalescing tips preserves exact observation evidence");require(store->lookup_observation("source-b",2,sequence,claim),"source-b batch claim");}
  auto next=batch("continue-batched-sources");next.observations={{"source-a",1,25,payload.digest,receipt.digest},{"source-b",2,25,payload.digest,receipt.digest}};commit(*store,next);
  next.operation_id="skip-batched-source";next.request_digest=sha256(next.operation_id);next.observations.front().sequence=27;
  expect(store->commit(next,committed).code==Code::conflict,"final coalesced tip still rejects a sequence gap");
  require(store->repair_index(),"rebuild coalesced source tips");AuditReport report;require(store->audit(report),"audit coalesced source tips and all claims");
  const auto duplicate=commit(*store,first);expect(duplicate.duplicate&&duplicate.generation==1,"original batched receipt remains exact after replay and rebuild");
}
void retention_and_fences(){
  Temporary temp("retention");auto store=open(temp.path);const auto child=object(std::string(1024U*1024U,'c'));const auto parent=object("root object",{child.digest});auto initial=batch("roots");initial.blobs={child,parent};initial.records={{"pin","current",parent.digest,{parent.digest},false}};commit(*store,initial);
  auto expiry=batch("expire-protected");expiry.expire_blobs={child.digest,parent.digest};CommitReceipt receipt;expect(store->commit(expiry,receipt).code==Code::conflict,"protected root rejects transitive expiry");
  expiry.operation_id="expire-unpinned";expiry.guards={{"pin","current",1,{}}};expiry.records={{"pin","current","",{},true}};commit(*store,expiry);const auto before=store->info().physical_bytes;const auto generation=store->generation();
  expect(store->reclaim_expired_blob(child.digest,generation-1).code==Code::conflict&&fs::exists(temp.path/"blobs"/child.digest),"stale reclaim fence preserves file");require(store->reclaim_expired_blob(child.digest,generation),"reclaim child");require(store->reclaim_expired_blob(parent.digest,store->generation()),"reclaim parent");expect(!fs::exists(temp.path/"blobs"/child.digest),"actual retained payload removed");expect(store->info().physical_bytes+900000<before,"released payload bytes reflected in quota");
  auto resurrection=batch("resurrect");resurrection.blobs={child};expect(store->commit(resurrection,receipt).code==Code::history_expired,"expired identities cannot silently resurrect");
  auto aba=batch("aba");aba.guards={{"pin","current",0,{}}};aba.records={{"pin","current","new",{},false}};expect(store->commit(aba,receipt).code==Code::conflict,"tombstone retains revision fence");store.reset();store=open(temp.path);BlobInfo info;require(store->blob_info(child.digest,info),"reclaimed metadata");expect(!info.present&&info.expired_generation!=0,"reclaimed state survives restart");AuditReport report;require(store->audit(report),"retention audit");
}
void index_scaling_and_rebuild(){
  Temporary temp("index");auto store=open(temp.path);
  for(unsigned page=0;page<8;++page){auto b=batch("page-"+std::to_string(page));for(unsigned i=0;i<64;++i){const auto n=page*64+i;auto key=std::to_string(10000+n);b.records.push_back({"retired",key,"historical-"+key,{},false});}commit(*store,b);}
  store.reset();store=open(temp.path);expect(native_index_statistics(*store).nodes_read<8,"healthy restart does not enumerate history");reset_native_index_statistics(*store);StoredRecord r;require(store->latest("retired","10000",r),"historical indexed point");const auto point=native_index_statistics(*store);expect(point.nodes_read<24&&point.point_queries==1,"point lookup logarithmic bound");reset_native_index_statistics(*store);
  std::vector<StoredRecord> records;bool more=false;require(store->scan_latest("retired","10","10250",17,records,more),"bounded historical range");expect(records.size()==17&&more&&records.front().record.key=="10251","ordered indexed range");expect(native_index_statistics(*store).nodes_read<64,"range work follows output plus index depth");
  const auto head=store->info().index_root;store.reset();fs::path node;for(const auto& dir:fs::directory_iterator(temp.path/"index")){if(dir.is_directory()&&fs::exists(dir.path()/head))node=dir.path()/head;}expect(!node.empty(),"find test index head");overwrite(node,"corrupted root node");store=open(temp.path);expect(!store->info().index_ready&&!store->info().writable,"corrupt index fails closed");require(store->repair_index(),"canonical index rebuild");require(store->latest("retired","10000",r),"historical record restored from canonical history");AuditReport report;require(store->audit(report),"rebuilt index audit");expect(report.generations==8&&report.records==512,"full retained history audit");
  store.reset();overwrite(temp.path/"INDEX-COVERAGE","incomplete watermark");store=open(temp.path);expect(!store->info().index_ready,"corrupt watermark cannot claim absence");require(store->repair_index(),"rebuild corrupt coverage");require(store->latest("retired","10511",r),"latest history after coverage rebuild");
  // Corruption below a valid root must not masquerade as a negative lookup or
  // evade an explicit repair that happens to have a current coverage marker.
  fs::path child;std::string child_key;const auto current_head=store->info().index_root;
  for(const auto& dir:fs::directory_iterator(temp.path/"index"))if(dir.is_directory())for(const auto& file:fs::directory_iterator(dir.path())){
    if(file.path().filename()==current_head)continue;
    std::ifstream in(file.path(),std::ios::binary);const std::string raw{std::istreambuf_iterator<char>(in),{}};std::vector<std::string> envelope,fields;
    if(unpack(raw,envelope,3)&&envelope.size()==3&&unpack(envelope[1],fields,8)&&fields.size()==8&&fields[0].starts_with(std::string("rretired\0",9))){child=file.path();child_key=fields[0].substr(9);break;}
  }
  expect(!child.empty(),"find non-root test index node");fs::remove(child);expect(store->latest("retired",child_key,r).code==Code::corrupt,"missing child is corruption, not absence");require(store->repair_index(),"repair missing child below current root");require(store->latest("retired",child_key,r),"restored child lookup");
}
void quota_and_corruption(){
  Temporary quota("quota");StoreOptions options;options.maximum_bytes=65536;auto store=open(quota.path,options);auto b=batch("too-large");b.blobs={object(std::string(30000,'x'))};CommitReceipt r;expect(store->commit(b,r).code==Code::capacity&&store->generation()==0,"quota rejects before canonical effects");expect(fs::is_empty(quota.path/"blobs"),"capacity rejection creates no orphan");
  Temporary corrupted("corruption");store=open(corrupted.path);auto value=object("authentic bytes");auto initial=batch("authentic");initial.blobs={value};commit(*store,initial);overwrite(corrupted.path/"blobs"/value.digest,"tampered bytes");Bytes out;expect(store->get_blob(value.digest,out).code==Code::corrupt,"artifact digest validation");
  store.reset();fs::remove(corrupted.path/"ROOT");std::unique_ptr<ICommitStore> missing;expect(open_native_store(corrupted.path.string(),{},missing).code==Code::corrupt,"missing authoritative root is not a new store");
  Temporary canonical("canonical-corrupt");auto canonical_store=open(canonical.path);auto first=batch("first-canonical");first.records={{"engine","historical","first",{},false}};commit(*canonical_store,first);commit(*canonical_store,batch("later-canonical"));overwrite(canonical.path/"transactions"/"0000000000000001.txn","damaged authoritative history");AuditReport audit;expect(canonical_store->audit(audit).code==Code::corrupt,"old canonical corruption detected by full audit");expect(canonical_store->repair_index().code==Code::corrupt&&!canonical_store->info().writable,"index repair cannot bless corrupt authoritative history");
  Temporary large("large-record");auto large_store=open(large.path);auto content=batch("large-record");content.records={{"payload","large",std::string(1024U*1024U,'x'),{},false}};commit(*large_store,content);auto small=batch("small-followup");small.records={{"payload","small","small",{},false}};commit(*large_store,small);std::uint64_t largest=0;for(const auto& entry:fs::recursive_directory_iterator(large.path/"index"))if(entry.is_regular_file())largest=std::max(largest,entry.file_size());expect(largest<8192,"large record payload is not copied into index paths");StoredRecord record;require(large_store->latest("payload","large",record),"large payload through canonical locator");expect(record.record.payload==content.records[0].payload,"canonical locator returns exact payload");
}
class GatedStore final:public ICommitStore {
public:
  ICommitStore& inner;std::mutex mutex;std::condition_variable cv;bool entered=false,released=false;
  explicit GatedStore(ICommitStore& v):inner(v){}
  Result commit(const CommitBatch& b,CommitReceipt& r)override{{std::unique_lock lock(mutex);entered=true;cv.notify_all();cv.wait(lock,[this]{return released;});}return inner.commit(b,r);}
  Result lookup_operation(std::string_view s,CommitReceipt& r)override{return inner.lookup_operation(s,r);}
  Result latest(std::string_view a,std::string_view b,StoredRecord& r)override{return inner.latest(a,b,r);}
  Result scan_latest(std::string_view a,std::string_view b,std::string_view c,std::size_t n,std::vector<StoredRecord>& r,bool& m)override{return inner.scan_latest(a,b,c,n,r,m);}
  Result get_blob(std::string_view d,Bytes& b)override{return inner.get_blob(d,b);}
  Result blob_info(std::string_view d,BlobInfo& b)override{return inner.blob_info(d,b);}
  Result list_blobs(std::uint64_t a,std::size_t n,std::vector<BlobInfo>& b,bool& m)override{return inner.list_blobs(a,n,b,m);}
  Result lookup_observation(std::string_view s,std::uint64_t e,std::uint64_t q,ObservationClaim& c)override{return inner.lookup_observation(s,e,q,c);}
  Result find_event(const EventKey& k,StoredEvent& e,std::string_view o,std::string_view p)override{return inner.find_event(k,e,o,p);}
  Result reclaim_expired_blob(std::string_view d,std::uint64_t g)override{return inner.reclaim_expired_blob(d,g);}
  Result repair_index()override{return inner.repair_index();}
  Result audit(AuditReport& r)override{return inner.audit(r);}
  StoreInfo info()const override{return inner.info();}
  std::uint64_t generation()const noexcept override{return inner.generation();}
};
void async_backpressure(){
  Temporary temp("worker");auto store=open(temp.path);GatedStore gate(*store);CommitWorker worker(gate,2);std::uint64_t first=0,second=0,rejected=0;require(worker.submit(batch("async-one"),first),"submit first");
  {std::unique_lock lock(gate.mutex);expect(gate.cv.wait_for(lock,std::chrono::seconds(5),[&]{return gate.entered;}),"worker entered storage");}
  require(worker.submit(batch("async-two"),second),"submit second");expect(worker.submit(batch("async-three"),rejected).code==Code::capacity,"pending+running reserve bounded completion slots");expect(worker.cached_info().generation==0,"cached owner view remains available during storage stall");
  {std::lock_guard lock(gate.mutex);gate.released=true;}gate.cv.notify_all();CommitCompletion completion;expect(worker.wait_complete(completion,10000),"first completion");expect(completion.ticket==first&&completion.result.committed(),"ordered first completion");expect(worker.wait_complete(completion,10000),"second completion");expect(completion.ticket==second&&completion.result.committed(),"ordered second completion");worker.stop();expect(worker.submit(batch("stopped"),rejected).code==Code::cancelled,"stopped worker rejects new work");expect(store->generation()==2,"accepted asynchronous work committed once");
}
class TestClock final:public IAdmissionClock {
public:
  mutable std::atomic<std::uint64_t> value{100};
  bool advance_on_read=false;
  std::uint64_t unix_seconds()const noexcept override{return value.fetch_add(advance_on_read?1U:0U);}
};
void expiry_admission(){
  Temporary admitted("admitted-expiry");TestClock clock;clock.advance_on_read=true;StoreOptions options;options.admission_clock=&clock;auto store=open(admitted.path,options);
  auto b=batch("authorized-transition");b.authorizations={{"peer-a","source-a/epoch/1",3,101}};b.records={{"engine","one","accepted",{},false}};const auto accepted=commit(*store,b);
  expect(accepted.admitted_unix_s==100&&accepted.authorizations.size()==1&&clock.value.load()==101,"admission clock read once and recorded before I/O");
  const auto retried=commit(*store,b);expect(retried.duplicate&&retried.admitted_unix_s==100&&clock.value.load()==101,"expiry after admission does not undo original commit/retry");
  auto late=batch("too-late");late.authorizations=b.authorizations;late.records={{"engine","one","late",{},false}};CommitReceipt receipt;expect(store->commit(late,receipt).code==Code::expired&&store->generation()==1,"expired before admission rejects all effects");
  clock.advance_on_read=false;clock.value.store(99);expect(store->commit(batch("rollback-clock"),receipt).code==Code::expired,"durable clock floor rejects time rollback");
  Temporary queued("queued-expiry");clock.value.store(100);auto queued_store=open(queued.path,options);GatedStore gate(*queued_store);CommitWorker worker(gate,1);std::uint64_t ticket=0;require(worker.submit(late,ticket),"queue expiring operation");
  {std::unique_lock lock(gate.mutex);expect(gate.cv.wait_for(lock,std::chrono::seconds(5),[&]{return gate.entered;}),"queued operation waits for owner admission");clock.value.store(101);gate.released=true;}gate.cv.notify_all();
  CommitCompletion completion;expect(worker.wait_complete(completion,10000)&&completion.result.code==Code::expired,"queued authorization expires before storage admission");worker.stop();expect(queued_store->generation()==0,"expired queued work leaves no canonical state");
}
}
void maintenance_headroom(){
  Temporary temp("maintenance-reserve");StoreOptions options;options.maximum_bytes=16U*1024U*1024U;auto store=open(temp.path,options);
  expect(store->info().maintenance_reserve_bytes==2U*1024U*1024U&&store->info().ordinary_limit_bytes==14U*1024U*1024U,"small-store maintenance reserve is bounded by absolute quota");
  const auto empty=object("");auto seed=batch("maintenance-empty");seed.blobs={empty};commit(*store,seed);
  std::vector<Digest> stored;CommitBatch blocked;bool stopped=false;
  for(unsigned i=0;i<32;++i){auto fill=batch("reserve-fill-"+std::to_string(i));auto bytes=std::string(1024U*1024U,'x');bytes.replace(0,std::to_string(i).size(),std::to_string(i));fill.blobs={object(std::move(bytes))};CommitReceipt receipt;const auto result=store->commit(fill,receipt);if(result.code==Code::capacity){blocked=std::move(fill);stopped=true;break;}require(result,"fill ordinary budget");stored.push_back(fill.blobs[0].digest);}
  expect(stopped&&!stored.empty(),"ordinary workload stops at reserved admission limit");const auto before=store->generation();
  auto review=batch("maintenance-review");review.commit_class=CommitClass::maintenance;review.records={{"maintenance","plan","expire first payload",{},false}};const auto reviewed=commit(*store,review);expect(reviewed.commit_class==CommitClass::maintenance&&reviewed.generation==before+1,"review commits using finite maintenance headroom");
  auto activate=batch("maintenance-activate");activate.commit_class=CommitClass::maintenance;activate.guards={{"maintenance","plan",1,{}}};activate.records={{"maintenance","plan","expiry active",{},false}};activate.expire_blobs={stored.front(),empty.digest};commit(*store,activate);require(store->reclaim_expired_blob(stored.front(),store->generation()),"reclaim while ordinary admission is stopped");commit(*store,blocked);
  bool maintenance_stopped=false;
  for(unsigned i=0;i<32;++i){auto fill=batch("maintenance-fill-"+std::to_string(i));fill.commit_class=CommitClass::maintenance;fill.records={{"maintenance","history-"+std::to_string(i),std::string(512U*1024U,'m'),{},false}};const auto generation=store->generation();CommitReceipt receipt;const auto result=store->commit(fill,receipt);if(result.code==Code::capacity){expect(store->generation()==generation,"exhausted reserve produces no partial canonical effect");maintenance_stopped=true;break;}require(result,"fill finite maintenance budget");}
  expect(maintenance_stopped&&store->info().physical_bytes<=options.maximum_bytes,"maintenance cannot bypass the absolute maximum");
  const auto accounted=store->info().physical_bytes;const auto generation=store->generation();store.reset();options.maximum_bytes=accounted+1024;store=open(temp.path,options);expect(store->reclaim_expired_blob(empty.digest,generation).code==Code::capacity&&store->generation()==generation,"native reclaim has no hidden metadata-quota bypass");
}
int main(){
  try{coverage_marker_paths();atomic_transitions();crash_matrix();strict_and_generic_history();coalesced_source_tips();retention_and_fences();index_scaling_and_rebuild();quota_and_corruption();async_backpressure();expiry_admission();maintenance_headroom();std::cout<<"canonical evidence: coverage compatibility, atomic transitions, crash cuts, indexed history, retention, corruption, bounded async, expiry admission and maintenance headroom passed\n";return 0;}
  catch(const std::exception& e){std::cerr<<"canonical evidence failure: "<<e.what()<<'\n';return 1;}
}

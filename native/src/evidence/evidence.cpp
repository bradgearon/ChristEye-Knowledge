#include "anaiero/sdk/evidence.hpp"
#include <condition_variable>
#include <chrono>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace anaiero::sdk::evidence {
Digest record_digest(const Record& r){return sha256(pack({r.kind,r.key,r.payload,pack(r.references),r.tombstone?"1":"0"}));}
Digest batch_digest(const CommitBatch& b){
  // Preconditions are an admission fence, not a different operation. A retry
  // may observe a newer global generation while preserving its exact effects.
  std::vector<std::string> records,blobs,observations,events;
  for(const auto& r:b.records)records.push_back(record_digest(r));
  for(const auto& v:b.blobs)blobs.push_back(pack({v.digest,sha256(v.bytes),pack(v.references)}));
  for(const auto& v:b.observations)observations.push_back(pack({v.source_uid,std::to_string(v.epoch),std::to_string(v.sequence),v.payload_digest,v.receipt_digest}));
  for(const auto& v:b.events)events.push_back(pack({v.key.kind,v.key.entity,v.key.clock,v.key.producer,std::to_string(v.key.source_tick),v.object_digest,v.origin}));
  if(b.authorizations.empty()&&b.commit_class==CommitClass::ordinary)return sha256(pack({"ANAIERO-BATCH/1",b.operation_id,b.request_digest,pack(records),pack(blobs),pack(observations),pack(events),pack(b.expire_blobs)}));
  std::vector<std::string> authorizations;for(const auto& a:b.authorizations)authorizations.push_back(pack({a.principal,a.scope,std::to_string(a.credential_generation),std::to_string(a.not_after_unix_s)}));
  if(b.commit_class!=CommitClass::ordinary)return sha256(pack({"ANAIERO-BATCH/3",b.operation_id,b.request_digest,pack(records),pack(blobs),pack(observations),pack(events),pack(b.expire_blobs),pack(authorizations),std::to_string(static_cast<unsigned>(b.commit_class))}));
  return sha256(pack({"ANAIERO-BATCH/2",b.operation_id,b.request_digest,pack(records),pack(blobs),pack(observations),pack(events),pack(b.expire_blobs),pack(authorizations)}));
}
class CommitWorker::Impl {
public:
  std::unique_ptr<ICommitStore> owned;
  ICommitStore* store=nullptr;
  const std::size_t capacity;
  mutable std::mutex mutex;
  std::condition_variable changed;
  std::deque<std::pair<std::uint64_t,CommitBatch>> pending;
  std::deque<CommitCompletion> completed;
  std::thread worker;
  std::function<void()> wakeup;
  StoreInfo cached;
  std::uint64_t next_ticket=1;
  std::size_t reserved=0;
  bool stopping=false;
  Impl(std::unique_ptr<ICommitStore> value,ICommitStore* borrowed,std::size_t cap):owned(std::move(value)),store(owned?owned.get():borrowed),capacity(cap){
    if(!store||capacity==0||capacity>256)throw std::invalid_argument("invalid commit worker store/capacity");
    cached=store->info();worker=std::thread([this]{run();});
  }
  ~Impl(){stop();}
  void run(){
    for(;;){
      std::pair<std::uint64_t,CommitBatch> request;
      {std::unique_lock lock(mutex);changed.wait(lock,[this]{return stopping||!pending.empty();});if(pending.empty()){if(stopping)return;continue;}request=std::move(pending.front());pending.pop_front();}
      CommitCompletion completion;completion.ticket=request.first;
      try{completion.result=store->commit(request.second,completion.receipt);}
      catch(const std::exception& e){completion.result={Code::io_error,e.what()};}
      catch(...){completion.result={Code::io_error,"unhandled storage worker exception"};}
      std::function<void()> signal;
      {std::lock_guard lock(mutex);cached=store->info();completed.push_back(std::move(completion));signal=wakeup;}
      changed.notify_all();if(signal){try{signal();}catch(...){/* A wakeup cannot erase the retained completion. */}}
    }
  }
  void stop(){
    {std::lock_guard lock(mutex);stopping=true;}changed.notify_all();
    if(worker.joinable())worker.join();
  }
};
CommitWorker::CommitWorker(std::unique_ptr<ICommitStore> p,std::size_t n):impl_(std::make_unique<Impl>(std::move(p),nullptr,n)){}
CommitWorker::CommitWorker(ICommitStore& p,std::size_t n):impl_(std::make_unique<Impl>(nullptr,&p,n)){}
CommitWorker::~CommitWorker()=default;
Result CommitWorker::submit(CommitBatch batch,std::uint64_t& ticket){
  std::lock_guard lock(impl_->mutex);ticket=0;
  if(impl_->stopping)return {Code::cancelled,"storage worker is stopping"};
  if(impl_->reserved>=impl_->capacity)return {Code::capacity,"storage queue/completion capacity exhausted"};
  if(batch.records.size()>1024||batch.blobs.size()>1024||batch.guards.size()>4096||batch.observations.size()>1024||batch.events.size()>1024||batch.expire_blobs.size()>1024||batch.authorizations.size()>16)return {Code::capacity,"storage queued batch count bound exceeded"};
  std::size_t bytes=0;const auto maximum=impl_->cached.maximum_batch_bytes;
  const auto add=[&](std::size_t size){if(bytes>maximum||size>maximum-bytes)return false;bytes+=size;return true;};
  if(!add(batch.operation_id.size())||!add(batch.request_digest.size()))return {Code::capacity,"storage queued batch byte bound exceeded"};
  for(const auto& r:batch.records){if(r.references.size()>4096||!add(r.kind.size())||!add(r.key.size())||!add(r.payload.size()))return {Code::capacity,"storage queued record bound exceeded"};for(const auto& d:r.references)if(!add(d.size()))return {Code::capacity,"storage queued reference bound exceeded"};}
  for(const auto& b:batch.blobs){if(b.references.size()>4096||!add(b.digest.size())||!add(b.bytes.size()))return {Code::capacity,"storage queued blob bound exceeded"};for(const auto& d:b.references)if(!add(d.size()))return {Code::capacity,"storage queued reference bound exceeded"};}
  for(const auto& g:batch.guards)if(!add(g.kind.size())||!add(g.key.size())||!add(g.expected_digest.size()))return {Code::capacity,"storage queued guard bound exceeded"};
  for(const auto& c:batch.observations)if(!add(c.source_uid.size())||!add(c.payload_digest.size())||!add(c.receipt_digest.size())||!add(16))return {Code::capacity,"storage queued claim bound exceeded"};
  for(const auto& e:batch.events)if(!add(e.key.kind.size())||!add(e.key.entity.size())||!add(e.key.clock.size())||!add(e.key.producer.size())||!add(e.origin.size())||!add(e.object_digest.size())||!add(8))return {Code::capacity,"storage queued event bound exceeded"};
  for(const auto& d:batch.expire_blobs)if(!add(d.size()))return {Code::capacity,"storage queued expiry bound exceeded"};
  for(const auto& a:batch.authorizations)if(!add(a.principal.size())||!add(a.scope.size())||!add(16))return {Code::capacity,"storage queued authorization bound exceeded"};
  if(impl_->next_ticket==0)return {Code::capacity,"storage ticket space exhausted"};
  ticket=impl_->next_ticket++;++impl_->reserved;impl_->pending.emplace_back(ticket,std::move(batch));impl_->changed.notify_one();return {};
}
bool CommitWorker::try_complete(CommitCompletion& completion){
  std::lock_guard lock(impl_->mutex);if(impl_->completed.empty())return false;
  completion=std::move(impl_->completed.front());impl_->completed.pop_front();--impl_->reserved;return true;
}
bool CommitWorker::wait_complete(CommitCompletion& completion,std::uint32_t timeout_ms){
  std::unique_lock lock(impl_->mutex);
  if(!impl_->changed.wait_for(lock,std::chrono::milliseconds(timeout_ms),[this]{return !impl_->completed.empty()||(impl_->stopping&&impl_->reserved==0);})||impl_->completed.empty())return false;
  completion=std::move(impl_->completed.front());impl_->completed.pop_front();--impl_->reserved;return true;
}
StoreInfo CommitWorker::cached_info()const{std::lock_guard lock(impl_->mutex);return impl_->cached;}
void CommitWorker::set_wakeup(std::function<void()> value){std::lock_guard lock(impl_->mutex);impl_->wakeup=std::move(value);}
void CommitWorker::stop(){impl_->stop();}
}

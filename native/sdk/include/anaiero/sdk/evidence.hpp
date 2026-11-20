#pragma once
#include "anaiero/sdk/types.hpp"
#include <functional>
#include <memory>
#include <limits>

namespace anaiero::sdk::evidence {
inline constexpr std::uint64_t any_generation=std::numeric_limits<std::uint64_t>::max();
struct Record {
  std::string kind,key;
  Bytes payload;
  std::vector<Digest> references;
  bool tombstone=false;
};
struct StoredRecord {
  Record record;
  std::uint64_t revision=0,generation=0;
  Digest digest;
};
struct RecordGuard {
  std::string kind,key;
  // Zero requires a never-created key. Tombstones retain their nonzero revision.
  std::uint64_t expected_revision=0;
  Digest expected_digest; // Empty omits the additional digest comparison.
};
struct Blob {
  Digest digest;
  Bytes bytes;
  std::vector<Digest> references;
};
struct ObservationClaim {
  std::string source_uid;
  std::uint64_t epoch=0,sequence=0;
  Digest payload_digest,receipt_digest;
};
struct EventKey {
  std::string kind,entity,clock,producer;
  std::uint64_t source_tick=0;
};
struct RegistryEvent {
  EventKey key;
  Digest object_digest;
  std::string origin;
};
struct StoredEvent {
  RegistryEvent event;
  std::uint64_t generation=0,position=0;
  Digest digest;
};
struct AuthorizationGuard {
  std::string principal,scope;
  std::uint64_t credential_generation=0,not_after_unix_s=0;
};
enum class CommitClass : std::uint32_t {ordinary=0,maintenance=1};
struct CommitBatch {
  std::string operation_id;
  Digest request_digest;
  std::uint64_t expected_generation=any_generation;
  std::vector<RecordGuard> guards;
  std::vector<Record> records;
  std::vector<Blob> blobs;
  std::vector<ObservationClaim> observations;
  std::vector<RegistryEvent> events;
  // Explicit canonical expiry. Protected latest-record/blob references reject
  // expiry. Physical reclamation is a separate recoverable operation.
  std::vector<Digest> expire_blobs;
  // Revision authority is enforced by RecordGuard. These bounded claims add
  // expiry at the serialized admission boundary; zero deadline is unbounded.
  std::vector<AuthorizationGuard> authorizations;
  CommitClass commit_class=CommitClass::ordinary;
};
struct CommitReceipt {
  std::string operation_id;
  Digest request_digest,batch_digest,root_digest;
  std::uint64_t generation=0;
  bool duplicate=false;
  std::uint64_t admitted_unix_s=0;
  std::vector<AuthorizationGuard> authorizations={};
  CommitClass commit_class=CommitClass::ordinary;
};
struct BlobInfo {
  Digest digest;
  std::uint64_t ordinal=0,bytes=0,created_generation=0,expired_generation=0;
  bool present=false;
  std::vector<Digest> references;
};
struct StoreInfo {
  std::string store_id,index_backend="native-sealed-avl";
  std::uint64_t generation=0,index_generation=0,blob_count=0,record_count=0;
  // Accounted serialized transaction/payload/index bytes plus fixed allowance,
  // not allocated filesystem blocks. An unreadable index saturates this value
  // to maximum_bytes. Filesystem free space is checked separately at admission.
  std::uint64_t physical_bytes=0,maximum_bytes=0;
  std::size_t maximum_batch_bytes=16U*1024U*1024U;
  std::uint64_t maintenance_reserve_bytes=0,ordinary_limit_bytes=0;
  Digest root_digest,index_root;
  bool writable=false,index_ready=false;
};
struct AuditReport {
  std::uint64_t generations=0,records=0,blobs=0,expired_blobs=0;
  std::uint64_t index_nodes_read=0,canonical_records_read=0;
};
struct IndexStatistics {
  std::uint64_t nodes_read=0,nodes_written=0,point_queries=0,range_queries=0;
};
class IAdmissionClock {
public:
  virtual ~IAdmissionClock()=default;
  virtual std::uint64_t unix_seconds() const noexcept=0;
};
struct StoreOptions {
  std::uint64_t maximum_bytes=1024ULL*1024ULL*1024ULL;
  std::uint64_t minimum_free_bytes=1024ULL*1024ULL;
  std::size_t maximum_batch_bytes=16U*1024U*1024U;
  std::size_t maximum_blob_bytes=8U*1024U*1024U;
  std::size_t maximum_records=128,maximum_blobs=64,maximum_references=256;
  std::size_t maximum_index_updates=1024;
  // Trusted host clock, borrowed for the store's lifetime. Null uses native UTC.
  const IAdmissionClock* admission_clock=nullptr;
  // Ordinary admission leaves min(this, maximum_bytes/8) for explicit bounded
  // maintenance. Maintenance itself still obeys the absolute maximum_bytes.
  std::uint64_t maintenance_reserve_bytes=16U*1024U*1024U;
};
enum class FaultPoint : std::uint32_t {
  none,after_objects,after_transaction,after_root,before_index,after_index
};
// Typed indexed reads are independent of the canonical writer and blob I/O.
// A missing/corrupt index cannot be interpreted as evidence of absence.
class IEvidenceIndex {
public:
  virtual ~IEvidenceIndex()=default;
  virtual Result lookup_operation(std::string_view,CommitReceipt&)=0;
  virtual Result latest(std::string_view kind,std::string_view key,StoredRecord&)=0;
  virtual Result scan_latest(std::string_view kind,std::string_view prefix,
      std::string_view after,std::size_t limit,std::vector<StoredRecord>&,bool& more)=0;
  virtual Result blob_info(std::string_view digest,BlobInfo&)=0;
  virtual Result list_blobs(std::uint64_t after,std::size_t limit,
      std::vector<BlobInfo>&,bool& more)=0;
  virtual Result lookup_observation(std::string_view source_uid,std::uint64_t epoch,
      std::uint64_t sequence,ObservationClaim&)=0;
  virtual Result find_event(const EventKey&,StoredEvent&,
      std::string_view exact_object={},std::string_view exact_origin={})=0;
  virtual std::uint64_t generation() const noexcept=0;
};
class ICommitStore:public IEvidenceIndex {
public:
  ~ICommitStore() override=default;
  virtual Result commit(const CommitBatch&,CommitReceipt&)=0;
  virtual Result get_blob(std::string_view digest,Bytes&)=0;
  virtual Result reclaim_expired_blob(std::string_view digest,
      std::uint64_t expected_generation)=0;
  virtual Result repair_index()=0;
  virtual Result audit(AuditReport&)=0;
  virtual StoreInfo info() const=0;
};
// Implemented in the native storage module; no OS/path type enters the SDK.
Result open_native_store(std::string_view directory,const StoreOptions&,
                         std::unique_ptr<ICommitStore>&);
void set_native_store_fault(ICommitStore&,FaultPoint) noexcept;
IndexStatistics native_index_statistics(const ICommitStore&) noexcept;
void reset_native_index_statistics(ICommitStore&) noexcept;
Digest record_digest(const Record&);
Digest batch_digest(const CommitBatch&);

struct CommitCompletion {std::uint64_t ticket=0;Result result;CommitReceipt receipt;};
class ICommitQueue {
public:
  virtual ~ICommitQueue()=default;
  virtual Result submit(CommitBatch,std::uint64_t& ticket)=0;
  virtual bool try_complete(CommitCompletion&)=0;
};
class CommitWorker final:public ICommitQueue {
public:
  explicit CommitWorker(std::unique_ptr<ICommitStore>,std::size_t capacity=8);
  explicit CommitWorker(ICommitStore&,std::size_t capacity=8);
  ~CommitWorker();
  CommitWorker(const CommitWorker&)=delete;
  CommitWorker& operator=(const CommitWorker&)=delete;
  // Admission reserves completion capacity as well. The wakeup is a signal-only
  // callback on the storage thread; domain completion runs on the consumer.
  Result submit(CommitBatch,std::uint64_t& ticket) override;
  bool try_complete(CommitCompletion&) override;
  bool wait_complete(CommitCompletion&,std::uint32_t timeout_ms);
  StoreInfo cached_info() const;
  void set_wakeup(std::function<void()>);
  void stop();
private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
}

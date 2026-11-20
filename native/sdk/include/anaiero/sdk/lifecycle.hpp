#pragma once
#include "anaiero/sdk/engine.hpp"
#include "anaiero/sdk/evidence.hpp"
#include <memory>

namespace anaiero::sdk::lifecycle {
enum class SourcePhase : std::uint32_t { registered=0,active,draining,sealed,retired };
enum class PeerPhase : std::uint32_t { active=0,suspended,revoked };
enum class SubscriptionPhase : std::uint32_t { active=0,suspended,expired,closed };
enum class PinKind : std::uint32_t { recovery=0,program,module,operation,reader,backup,operator_pin,bootstrap_continuation };
enum class BootstrapContinuationPhase : std::uint32_t { pending=0,accepted,cancelled,expired };
enum class HistoryAvailability : std::uint32_t { retained=0,archived,expired };
enum class MaintenancePhase : std::uint32_t { reviewed=0,activated,completed };
enum class SourceAuthorityKind : std::uint32_t { local=0,relay=1 };

struct SourceAuthority {
  std::string uid,owner,clock_domain;
  SourceAuthorityKind kind=SourceAuthorityKind::local;
};

struct Options {
  std::size_t max_active_sources=32,max_metrics=256,max_active_peers=32,
              max_active_subscriptions=32,max_pins=64,max_pin_objects=64;
  std::uint64_t maximum_lease_seconds=30ULL*24ULL*60ULL*60ULL;
  // 15 leaves room for atomic expiry with all 32 peers/subscriptions and
  // installed roots in the native store's default 128-record transaction.
  std::size_t max_bootstrap_continuations=15;
};
struct SourceRef {
  std::string uid;
  std::uint64_t epoch=0;
  bool operator==(const SourceRef&) const=default;
};
struct Source {
  SourceRef id;
  EngineIdentity identity;
  std::string owner;
  SourcePhase phase=SourcePhase::registered;
  std::uint64_t engine_revision=0,last_sequence=0,last_tick=0;
  Digest last_payload_sha256,state_sha256,state_object_sha256;
};
struct Cursor {
  std::uint64_t sequence=0;
  Digest payload_sha256;
  bool operator==(const Cursor&) const=default;
};
// Content identity includes the stream and sequence; equal numerical values in
// different streams do not accidentally share a history-retention identity.
Bytes encode_source_observation(const SourceRef&,std::uint64_t sequence,const Observation&);
Result decode_source_observation(std::string_view,SourceRef&,std::uint64_t& sequence,Observation&);
struct Metric {
  SourceRef source;
  Observation observation;
  std::uint64_t sequence=0;
};
struct Snapshot {
  std::uint32_t schema=2;
  std::uint64_t revision=0,lifetime_observations=0;
  std::vector<Source> sources;
  std::vector<Metric> metrics;
  std::size_t active_peers=0,active_subscriptions=0,pins=0;
  bool quarantined=false;
  std::string incarnation;
  Digest recovery_policy_sha256;
  std::size_t pending_bootstraps=0;
};
// Accounted capacity at one canonical generation. These values describe the
// configured quota, not filesystem allocation/free space or a reservation for a
// future transaction. Protected authority/retry/tombstone identities remain
// retained; source retirement does not promise to reclaim their metadata.
struct CapacityStatus {
  std::uint64_t generation=0,accounted_bytes=0,maximum_bytes=0;
  std::uint64_t ordinary_limit_bytes=0,maintenance_reserve_bytes=0;
  std::uint64_t ordinary_remaining_bytes=0,maintenance_remaining_bytes=0;
  std::uint64_t pending_reclamation_bytes=0;
  bool writable=false,index_ready=false;
};
struct Peer {
  std::string principal;
  Digest certificate_sha256;
  std::uint64_t generation=0,expires_unix_s=0;
  PeerPhase phase=PeerPhase::active;
};
struct Subscription {
  std::string id,principal;
  SourceRef scope;
  std::uint64_t generation=0,lease_until_unix_s=0;
  SubscriptionPhase phase=SubscriptionPhase::active;
  Cursor acknowledged,offered;
  bool has_offer=false;
};
struct Pin {
  std::string id,owner;
  PinKind kind=PinKind::operator_pin;
  std::uint64_t generation=0,expires_unix_s=0;
  std::vector<Digest> objects;
};
struct MaintenanceRequest {
  std::string id;
  SourceRef source;
  std::uint64_t expire_through=0,now_unix_s=0;
  HistoryAvailability disposition=HistoryAvailability::expired;
  std::string archive_directory;
  std::vector<Digest> objects;
};
struct MaintenancePlan {
  MaintenanceRequest request;
  MaintenancePhase phase=MaintenancePhase::reviewed;
  std::uint64_t protected_revision=0,subscription_revision=0,source_sequence=0,
                expected_bytes=0,new_floor=0;
  Digest protected_root_sha256,source_tip_sha256;
};
struct Bootstrap {
  std::string subscription_id,principal;
  std::uint64_t generation=0,lease_until_unix_s=0;
  SourceRef scope;
  Cursor tip;
  Digest snapshot_sha256;
  Bytes snapshot;
};
// An explicit pending checkpoint-to-stream handoff. Its dedicated pin kind
// consumes one ordinary pin slot and one bounded continuation slot. Metadata
// remains as an immutable outcome after those resource protections are released.
struct BootstrapContinuation {
  std::string id,subscription_id,principal;
  EngineIdentity source;
  std::uint64_t generation=0,lease_until_unix_s=0,credential_generation=0;
  Cursor tip;
  Digest checkpoint_sha256,state_sha256;
  BootstrapContinuationPhase phase=BootstrapContinuationPhase::pending;
};
struct ArchiveOptions {
  std::uint64_t maximum_bytes=1024ULL*1024ULL*1024ULL;
  std::size_t maximum_object_bytes=8U*1024U*1024U;
};
struct IArchiveStore {
  virtual ~IArchiveStore()=default;
  virtual std::string_view identifier() const noexcept=0;
  virtual Result put(std::string_view digest,std::string_view bytes)=0;
  virtual Result get(std::string_view digest,Bytes&)=0;
};
Result open_native_archive(std::string_view directory,const ArchiveOptions&,
                           std::unique_ptr<IArchiveStore>&);
struct RestoreReport {
  std::uint64_t copied_files=0,copied_bytes=0,source_generation=0;
  Digest source_root,incarnation;
  std::string staging_directory;
  bool published=false;
};
struct RestoreOptions {
  evidence::StoreOptions store;
  Options lifecycle;
  std::size_t maximum_files=262144,maximum_file_bytes=64U*1024U*1024U;
};
// Quiescent native store clone. Source bytes remain unchanged. The destination
// appears only after the clone is audited and its quarantine barrier commits.
// Existing destinations are never overwritten; a retained matching intent can
// reconcile an uncertain successful publication with the same operation id.
Result restore_quarantined_store(std::string_view source,std::string_view destination,
    std::string operation_id,RestoreReport&,const RestoreOptions& options={});

// Domain service over the canonical commit store. It never owns a separate
// authority log. The host serializes calls; all mutations use store CAS guards.
class Service {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  explicit Service(evidence::ICommitStore&,Options options={},IArchiveStore* archive=nullptr);
  ~Service();
  Service(const Service&)=delete;
  Service& operator=(const Service&)=delete;
  Result open();
  Result snapshot(Snapshot&) const;
  Result capacity(CapacityStatus&) const;
  Result source(const SourceRef&,Source&) const;
  Result source_authority(std::string_view uid,SourceAuthority&) const;
  // One immutable UID owner/clock/role across local engines and authenticated
  // relay streams. Stage in the caller's SAME canonical transaction so racing
  // first registrations cannot establish separate ownership authorities.
  Result stage_source_authority(evidence::CommitBatch&,const SourceAuthority&);
  Result register_engine(const EngineIdentity&,std::string owner,std::string operation_id);
  Result activate(const SourceRef&,std::string operation_id);
  Result begin_drain(const SourceRef&,std::string operation_id);
  Result seal(const SourceRef&,const Cursor& expected_tip,std::string operation_id);
  Result retire(const SourceRef&,std::string operation_id);
  // Enriches the SAME engine transaction with the lifecycle fence, canonical
  // source tip and active checkpoint. No state is advanced before store commit.
  Result stage_commit(evidence::CommitBatch&,const PreparedTransition&);
  Result upsert_peer(const Peer&,std::uint64_t expected_generation,
                     std::uint64_t now_unix_s,std::string operation_id);
  Result peer(std::string_view principal,Peer&) const;
  Result open_subscription(const Subscription&,std::uint64_t now_unix_s,
                           std::string operation_id);
  Result subscription(std::string_view id,Subscription&) const;
  Result offer(std::string_view id,std::uint64_t generation,const Cursor&,
               std::uint64_t now_unix_s,std::string operation_id);
  Result acknowledge(std::string_view id,std::uint64_t generation,const Cursor&,
                     std::uint64_t now_unix_s,std::string operation_id);
  Result close_subscription(std::string_view id,std::uint64_t generation,
                            std::string operation_id);
  Result expire_subscriptions(std::uint64_t now_unix_s,std::string operation_id);
  Result expire_peer_leases(std::uint64_t now_unix_s,std::string operation_id);
  Result put_pin(const Pin&,std::uint64_t expected_generation,
                 std::uint64_t now_unix_s,std::string operation_id);
  Result release_pin(std::string_view id,std::uint64_t expected_generation,
                     std::string operation_id);
  Result pin(std::string_view id,Pin&) const;
  Result review_maintenance(const MaintenanceRequest&,std::string operation_id,
                            MaintenancePlan&);
  Result maintenance(std::string_view id,MaintenancePlan&) const;
  Result activate_maintenance(std::string_view id,std::uint64_t now_unix_s,
                              std::string operation_id);
  Result execute_maintenance(std::string_view id,std::string operation_id);
  Result replay_status(const SourceRef&,std::uint64_t after_sequence) const;
  Result accept_bootstrap(const Bootstrap&,std::uint64_t now_unix_s,
                          std::string operation_id);
  Result bootstrap_continuation(std::string_view id,BootstrapContinuation&) const;
  Result pending_bootstrap(std::string_view subscription_id,BootstrapContinuation&) const;
  // Stage into the caller's SAME canonical transaction as its protocol offer
  // or acceptance/release witness. The caller supplies the operation identity.
  Result stage_bootstrap_continuation(evidence::CommitBatch&,const BootstrapContinuation&,
                                      std::uint64_t now_unix_s);
  Result stage_accept_bootstrap_continuation(evidence::CommitBatch&,std::string_view id,
      const Bootstrap&,std::uint64_t now_unix_s);
  Result stage_cancel_bootstrap_continuation(evidence::CommitBatch&,std::string_view id);
  // Stage scoped grant withdrawal and its active-directory guard in the
  // protocol's same authority-change transaction.
  Result stage_revoke_bootstrap_scope(evidence::CommitBatch&,std::string_view principal,const SourceRef&);
  Result installed_bootstrap(std::string_view subscription_id,Bootstrap&) const;
  // Restored stores remain unable to execute or grant replay until an explicit
  // local operator supplies their current policy witness. The witness is an
  // operator declaration, not a proof that the backup was the newest root.
  Result quarantine_restore(std::string operation_id,std::string_view restore_nonce={});
  Result reauthorize_restore(std::string_view current_policy_sha256,
                             std::string operation_id);
  // Original identities captured in the reauthorization commit, so post-commit
  // engine-root cleanup can resume even after the live view/configuration changed.
  Result reauthorization_sources(std::string_view operation_id,
                                 std::vector<EngineIdentity>&) const;
};
std::string_view phase_name(SourcePhase) noexcept;
std::string_view phase_name(SubscriptionPhase) noexcept;
}

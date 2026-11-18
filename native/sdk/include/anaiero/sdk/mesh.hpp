#pragma once
#include "anaiero/sdk/lifecycle.hpp"

namespace anaiero::sdk::mesh {
// Authentication is asserted only by an authenticated transport adapter. Scope
// and current authority are checked separately against the canonical store.
struct AuthenticatedIdentity {
  Digest certificate_sha256;
  std::uint64_t certificate_expiry_unix_s=0;
  bool verified=false;
};
struct Session {
  std::string principal;
  Digest certificate_sha256;
  std::uint64_t credential_generation=0,certificate_expiry_unix_s=0,authority_expiry_unix_s=0;
};
struct Grant {
  std::string principal;
  EngineIdentity source;
  std::uint64_t generation=1;
  bool send=false,receive=false,bootstrap=false;
  std::vector<Digest> artifacts;
};
struct Options {
  std::size_t maximum_page_observations=16,maximum_page_bytes=24576;
  std::size_t maximum_artifact_bytes=8U*1024U*1024U,maximum_artifacts=32;
};
struct DeliveryItem {evidence::ObservationClaim claim;Bytes payload;};
struct Delivery {
  std::string subscription_id;
  std::uint64_t generation=0;
  EngineIdentity source;
  lifecycle::Cursor from,to;
  std::vector<DeliveryItem> observations;
};
struct ReceiveReceipt {
  lifecycle::Cursor cursor;
  std::uint64_t generation=0,store_generation=0;
  Digest receipt_sha256;
  bool duplicate=false;
};
struct TransferCost {std::uint64_t bytes=0,work=0;};
struct ArtifactChunk {
  Digest digest;
  std::uint64_t offset=0,total_bytes=0;
  Bytes bytes;
};
struct BootstrapOffer {
  std::string id,subscription_id;
  std::uint64_t generation=0,lease_until_unix_s=0;
  EngineIdentity source;
  lifecycle::Cursor tip;
  Digest checkpoint_sha256;
  Bytes checkpoint;
};
// Trusted local receiver retention is explicit and bounded. An observation
// interval must contain complete received pages (at most 32 observations).
// Artifact/checkpoint expiry never removes source authority or retry identities.
struct ReceiverRetentionRequest {
  std::string id,principal;
  lifecycle::SourceRef source;
  std::uint64_t first_sequence=0,last_sequence=0;
  std::vector<Digest> artifacts;
  std::vector<std::string> bootstrap_ids;
  // Extra copies received under overlapping subscriptions are selected by
  // their returned receipt digest, without an unbounded historical scan.
  std::vector<Digest> delivery_receipts={};
};
struct ReceiverRetentionPlan {
  ReceiverRetentionRequest request;
  lifecycle::MaintenancePhase phase=lifecycle::MaintenancePhase::reviewed;
  std::string store_id;
  Digest reviewed_root,candidate_sha256;
  std::uint64_t reviewed_generation=0,expected_bytes=0;
  std::vector<Digest> objects;
};
Bytes encode_delivery(const Delivery&);
Result decode_delivery(std::string_view,Delivery&,const Options& options={});
Bytes encode_grant(const Grant&);
Result decode_grant(std::string_view,Grant&);
Bytes encode_bootstrap(const BootstrapOffer&);
Result decode_bootstrap(std::string_view,BootstrapOffer&,std::size_t maximum_bytes=8U*1024U*1024U);

// All calls execute on the same serialized storage lane as engine commits.
// No transport, OS handle or background thread is owned by this domain service.
// Receive receipts identify the authenticated relay; they never assert an
// original-author signature which has not been independently verified.
class Service {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  explicit Service(evidence::ICommitStore&,Options options={});
  ~Service();
  Result authenticate(const AuthenticatedIdentity&,std::string_view principal,
                      std::uint64_t now_unix_s,Session&) const;
  Result authorize(const Session&,const lifecycle::SourceRef&,bool sending,
                   std::uint64_t now_unix_s,Grant* grant=nullptr) const;
  Result authorize_delivery(const Session&,const Delivery&,std::uint64_t now_unix_s) const;
  Result authorize_artifact(const Session&,const lifecycle::SourceRef&,std::string_view digest,
                            std::uint64_t now_unix_s) const;
  Result authorize_bootstrap(const Session&,const BootstrapOffer&,std::uint64_t now_unix_s) const;
  Result bootstrap_accepted(const Session&,std::string_view subscription_id,std::uint64_t generation,
                            std::string_view checkpoint_sha256,const lifecycle::Cursor&,
                            std::uint64_t now_unix_s) const;
  Result configure_grant(const Grant&,std::uint64_t expected_generation,
                         std::string operation_id);
  Result grant(std::string_view principal,const lifecycle::SourceRef&,Grant&) const;
  Result subscribe(const Session&,const lifecycle::Subscription&,
                   std::uint64_t now_unix_s);
  Result next(const Session&,std::string_view subscription_id,std::uint64_t generation,
              std::uint64_t now_unix_s,Delivery&,TransferCost&);
  Result acknowledge(const Session&,const Delivery&,std::uint64_t now_unix_s);
  Result receive(const Session&,const Delivery&,std::uint64_t now_unix_s,ReceiveReceipt&);
  Result received(std::string_view principal,std::string_view subscription_id,
                  const lifecycle::SourceRef&,ReceiveReceipt&) const;
  // Relay evidence has its own indexed canonical namespace. Installing a
  // bootstrap advances its cursor without inventing locally observed events.
  Result received_observation(std::string_view principal,const lifecycle::SourceRef&,
                              std::uint64_t sequence,DeliveryItem&) const;
  Result artifact_chunk(const Session&,const lifecycle::SourceRef&,std::string_view digest,
                        std::uint64_t offset,std::size_t maximum_bytes,
                        std::uint64_t now_unix_s,ArtifactChunk&,TransferCost&);
  Result receive_artifact(const Session&,const lifecycle::SourceRef&,std::string_view digest,
                          Bytes bytes,std::uint64_t now_unix_s,ReceiveReceipt&);
  Result offer_bootstrap(const Session&,std::string subscription_id,std::uint64_t generation,
                        const lifecycle::SourceRef&,std::uint64_t lease_until_unix_s,
                        std::uint64_t now_unix_s,BootstrapOffer&);
  Result install_bootstrap(const Session&,const BootstrapOffer&,std::uint64_t now_unix_s,
                          ReceiveReceipt&);
  Result accept_bootstrap(const Session&,const BootstrapOffer&,std::uint64_t now_unix_s);
  // Trusted local maintenance; releasing a live transfer must follow transport
  // cancellation. Expiry alone does not assert that a reader released its data.
  Result cancel_bootstrap(std::string_view offer_id,std::string operation_id);
  // Review is a canonical immutable plan; activation rejects every intervening
  // store mutation. Current cursors/projections, live pins and all other roots
  // remain protective. Reclamation can resume after interruption; expired
  // reads/retries return HISTORY_EXPIRED instead of recreating accepted data.
  Result review_receiver_retention(const ReceiverRetentionRequest&,
                                  std::string operation_id,ReceiverRetentionPlan&);
  Result receiver_retention(std::string_view plan_id,ReceiverRetentionPlan&) const;
  Result activate_receiver_retention(std::string_view plan_id,std::string operation_id);
  Result reclaim_receiver_retention(std::string_view plan_id,std::string operation_id);
};

// Cost-aware deficit round robin. Each connection has one queued bounded unit;
// repeated connections share principal and source round budgets. The adapter
// also reserves a separate bounded control queue for close and completion.
struct ScheduleKey {std::uint64_t connection=0;std::string principal,source;};
struct SchedulerOptions {
  std::size_t maximum_connections=8;
  std::uint64_t byte_quantum=32768,work_quantum=64;
  std::uint64_t principal_byte_budget=65536,source_byte_budget=65536;
  std::uint64_t principal_work_budget=4096,source_work_budget=4096;
};
class FairScheduler {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  explicit FairScheduler(SchedulerOptions options={});
  ~FairScheduler();
  Result enqueue(ScheduleKey,TransferCost);
  bool take(std::uint64_t& connection);
  void cancel(std::uint64_t connection);
  std::size_t queued() const noexcept;
};
}

#pragma once
// A receipt attests local durable receipt through an enrolled connection.
// It is not a transferable signature by the original observation author.
#include "anaiero/contracts/mesh_replay.hpp"
#include <string>
namespace anaiero::mesh {
enum class IngressKind : std::uint8_t {mutual_tls=1,local_import=2};
struct IngressReceipt {
  AuthorizedPeer peer{};
  std::string certificate_sha256,payload_sha256,receipt_sha256;
  std::uint64_t policy_revision=0,received_unix_s=0,source_sequence=0;
  Scope scope{};
  IngressKind kind=IngressKind::mutual_tls;
};
struct IMeshIngressStore {
  virtual ~IMeshIngressStore()=default;
  virtual ReplayCode check(const runtime::Observation&,bool& duplicate)noexcept=0;
  // The first receipt is immutable. Repeated identical bytes under the same
  // principal reuse its original certificate, policy and receipt time.
  virtual ReplayCode prepare(IngressReceipt&,bool evidence_exists)noexcept=0;
  virtual ReplayCode commit(const runtime::Observation&,ReplayCursor&)noexcept=0;
};
struct IMeshReceiptVerifier {
  virtual ~IMeshReceiptVerifier()=default;
  // Called for evidence already read from its committed journal/checkpoint.
  virtual ReplayCode verify_committed(const runtime::Observation&)const noexcept=0;
};
struct IMeshIngressProjection {
  virtual ~IMeshIngressProjection()=default;
  virtual bool prepare(const runtime::Observation&)noexcept=0;
  virtual bool apply(const runtime::Observation&)noexcept=0;
};
struct IngressResult {
  ReplayCode code=ReplayCode::invalid;
  bool duplicate=false;
  ReplayCursor committed{};
  IngressReceipt receipt{};
};
} // namespace anaiero::mesh

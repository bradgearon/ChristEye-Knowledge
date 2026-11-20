#include "anaiero/sdk/session_evidence_service.hpp"

namespace anaiero::sdk::evidence {
Result SessionEvidenceService::stage_transition(const PreparedTransition& transition,
    std::uint64_t record_revision,const Digest& record_digest,
    const EngineState& committed_state,CommitBatch& batch,Record& latest) const {
  batch.operation_id=transition.operation_id;
  batch.request_digest=transition.request_sha256;
  if(record_revision&&transition.observations.empty()&&
     (transition.target.phase==Phase::paused||transition.target.phase==Phase::cancelled)&&
     transition.target.tick==committed_state.tick&&
     transition.target.checkpoint==committed_state.checkpoint&&
     transition.target.configuration==committed_state.configuration)
    batch.commit_class=CommitClass::maintenance;
  const auto state=encode_checkpoint(transition.identity,transition.target);
  const auto prepared=encode_transition(transition);
  if(state.empty()||prepared.empty())return {Code::capacity,"canonical transition encoding bound"};
  const auto state_sha=sha256(state),transition_sha=sha256(prepared);
  batch.blobs.push_back({state_sha,state,{}});
  batch.blobs.push_back({transition_sha,prepared,{}});
  latest={"sdk.engine",transition.identity.instance_id,
    encode_session_engine_record({transition.identity,state_sha,transition_sha}),
    {state_sha,transition_sha},false};
  if(latest.payload.empty())return {Code::corrupt,"invalid session engine record"};
  batch.records.push_back(latest);
  batch.guards.push_back({"sdk.engine",transition.identity.instance_id,record_revision,record_digest});
  const auto prefix=metric_prefix(transition.identity);
  for(const auto& observation:transition.observations) {
    auto bytes=encode_session_metric_record({transition.identity,observation,transition.target.revision});
    if(bytes.empty())return {Code::corrupt,"invalid session metric record"};
    batch.records.push_back({"sdk.metric",prefix+observation.metric,std::move(bytes),{},false});
  }
  return {};
}
Result SessionEvidenceService::read_checkpoint(const Digest& checkpoint_sha256,
                                                 const EngineIdentity& expected,
                                                 EngineState& output) const {
  Bytes data;
  if(auto fetched=store_.get_blob(checkpoint_sha256,data);!fetched)return fetched;
  if(sha256(data)!=checkpoint_sha256)return {Code::corrupt,"retained state digest mismatch"};
  EngineIdentity stored;
  EngineState state;
  if(auto parsed=decode_checkpoint(data,stored,state);!parsed)return parsed;
  if(stored!=expected)return {Code::corrupt,"retained checkpoint source identity mismatch"};
  output=std::move(state);return {};
}
Result SessionEvidenceService::registration(const EngineIdentity& identity,
                                             const EngineState& state,
                                             PreparedTransition& transition) const {
  if(!valid_identity(identity)||!valid_state(state))
    return {Code::invalid,"invalid engine registration identity or state"};
  transition={};
  transition.identity=identity;
  transition.target=state;
  transition.expected_revision=state.revision;
  transition.expected_state_sha256=state_digest(state);
  transition.operation_id="register-"+sha256(encode_identity(identity));
  transition.request_sha256=sha256(pack({"register/1",encode_identity(identity),encode_state(state)}));
  return {};
}
Digest SessionEvidenceService::phase_request(const EngineIdentity& identity,
                                              std::string_view operation,Phase phase) const {
  return sha256(pack({"phase-control/1",encode_identity(identity),
    std::string(operation),std::string(phase_name(phase))}));
}
Digest SessionEvidenceService::detach_request(const EngineIdentity& identity) const {
  return sha256(pack({"detach/1",encode_identity(identity)}));
}
std::string SessionEvidenceService::metric_prefix(const EngineIdentity& identity) const {
  return sha256(pack({identity.source_uid,std::to_string(identity.epoch)}))+"/";
}
} // namespace anaiero::sdk::evidence

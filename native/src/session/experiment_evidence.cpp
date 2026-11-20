#include "experiment_evidence.hpp"
#include "anaiero/sdk/session_evidence_service.hpp"
#include <utility>
namespace anaiero::sdk::experiment_evidence {
namespace {
Result corrupt(std::string why){return {Code::corrupt,std::move(why)};}
}
Result canonical_engine(evidence::ICommitStore& store,std::string_view instance,
                        evidence::StoredRecord& record,EngineIdentity& identity,
                        EngineState& state,PreparedTransition& transition,std::vector<Digest>& references){
  if(auto r=store.latest("sdk.engine",instance,record);!r)return r;
  if(record.record.tombstone)return {Code::expired,"engine is retired"};
  SessionEngineRecord decoded;
  if(auto r=decode_session_engine_record(record.record.payload,decoded);!r)return r;
  identity=decoded.identity;
  if(identity.instance_id!=instance)return corrupt("canonical engine instance mismatch");
  references={decoded.checkpoint_sha256,decoded.transition_sha256};
  if(record.record.references!=references)return corrupt("canonical engine references changed");
  evidence::SessionEvidenceService session(store);
  if(auto r=session.read_checkpoint(decoded.checkpoint_sha256,identity,state);!r)return r;
  Bytes encoded;
  if(auto r=store.get_blob(decoded.transition_sha256,encoded);!r)return r;
  if(sha256(encoded)!=decoded.transition_sha256||!decode_transition(encoded,transition)||
     transition.identity!=identity||transition.target!=state)
    return corrupt("canonical engine evidence mismatch");
  return {};
}
Result observations(evidence::ICommitStore& store,const EngineIdentity& identity,const EngineState& state,
                    std::vector<Observation>& out,std::vector<evidence::RecordGuard>& guards){
  const auto prefix=evidence::SessionEvidenceService(store).metric_prefix(identity);
  std::string after;bool more=true;
  while(more){
    std::vector<evidence::StoredRecord> page;
    if(auto r=store.scan_latest("sdk.metric",prefix,after,32,page,more);!r)return r;
    if(more&&page.empty())return corrupt("outcome metric index made no progress");
    for(const auto& record:page){
      after=record.record.key;
      if(record.record.tombstone)continue;
      SessionMetricRecord item;
      if(auto r=decode_session_metric_record(record.record.payload,item);!r)return r;
      if(item.identity!=identity||item.revision>state.revision||item.observation.tick>state.tick)
        return corrupt("canonical outcome metric disagrees with engine state");
      if(out.size()==128)return {Code::capacity,"outcome metric bound"};
      out.push_back(std::move(item.observation));
      guards.push_back({"sdk.metric",record.record.key,record.revision,record.digest});
    }
  }
  return {};
}
}

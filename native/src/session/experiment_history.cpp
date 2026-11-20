#include "anaiero/sdk/experiment_history.hpp"
#include "experiment_history_codec.hpp"
#include "experiment_evidence.hpp"
#include <algorithm>
#include <charconv>
namespace anaiero::sdk {
namespace {
namespace ev=evidence;
constexpr std::string_view intent_kind="sdk.experiment-intent",result_kind="sdk.experiment-result",outcome_kind="sdk.experiment-outcome";
using history_codec::bad;using history_codec::strategy_bytes;using history_codec::strategy_read;
using history_codec::spec_bytes;using history_codec::spec_read;
using history_codec::result_bytes;using history_codec::result_read;
using history_codec::review_bytes;using history_codec::review_read;
Result object(ev::ICommitStore& store,std::string_view kind,std::string_view key,ev::StoredRecord& record,Bytes& bytes){
  if(auto r=store.latest(kind,key,record);!r)return r;
  if(record.record.tombstone)return {Code::history_expired,"experiment roots were released"};
  if(!valid_digest(record.record.payload))return bad("experiment object address");
  return store.get_blob(record.record.payload,bytes);
}
Result publish(ev::ICommitStore& store,std::string_view kind,std::string_view id,std::string operation,Bytes bytes,
               std::vector<Digest> references,std::vector<ev::RecordGuard> guards={}){
  if(bytes.empty()||bytes.size()>8U*1024U*1024U)return {Code::capacity,"recorded experiment object exceeds canonical encoding or 8 MiB bound"};
  ev::CommitBatch batch;batch.operation_id=std::move(operation);const auto digest=sha256(bytes);batch.request_digest=digest;
  batch.blobs.push_back({digest,std::move(bytes),std::move(references)});
  batch.guards=std::move(guards);batch.guards.push_back({std::string(kind),std::string(id),0,{}});
  batch.records.push_back({std::string(kind),std::string(id),digest,{digest},false});
  ev::CommitReceipt receipt;return store.commit(batch,receipt);
}
Result read_intent(ev::ICommitStore& store,std::string_view id,RecordedExperiment& value,ev::StoredRecord& record){
  Bytes bytes;std::vector<std::string> f;
  if(auto r=object(store,intent_kind,id,record,bytes);!r)return r;
  if(!unpack(bytes,f,4)||f.size()!=4||f[0]!="recorded-experiment/1"||!spec_read(f[1],value.specification)||
     value.specification.id!=id||!decode_checkpoint(f[2],value.source,value.source_state)||!strategy_read(f[3],value.strategy))return bad("recorded experiment intent");
  return {};
}
}
Result ExperimentHistoryService::load(std::string_view id,RecordedExperiment& output){
  if(!valid_token(id,80))return {Code::invalid,"experiment ID"};
  RecordedExperiment value;ev::StoredRecord intent;
  if(auto r=read_intent(store_,id,value,intent);!r)return r;
  ev::StoredRecord result;Bytes bytes;const auto found=object(store_,result_kind,id,result,bytes);
  if(found){
    value.result.source=value.source;value.result.source_revision=value.source_state.revision;
    value.result.source_state_sha256=state_digest(value.source_state);value.result.strategy=value.strategy;
    if(auto r=result_read(bytes,value.result);!r)return r;
    if(value.result.candidates.size()!=value.specification.candidates.size())return bad("recorded experiment candidate count changed");
    for(std::size_t i=0;i<value.result.candidates.size();++i)
      if(value.result.candidates[i].id!=value.specification.candidates[i].id)return bad("recorded experiment candidate identity changed");
    value.completed=true;
  }else if(found.code!=Code::not_found)return found;
  output=std::move(value);return {};
}
Result ExperimentHistoryService::select(IEngine& source,const ExperimentSpecification& specification,
    const IScoreStrategy& strategy,RecordedExperiment& output){
  if(!valid_token(specification.id,80)||specification.inputs.empty()||specification.inputs.size()>128||
     specification.candidates.empty()||specification.candidates.size()>8)return {Code::invalid,"experiment identity/count bounds"};
  std::size_t aggregate=0;
  auto admit=[&](const Envelope& value){
    if(!valid_token(value.schema)||value.payload.size()>4U*1024U*1024U||aggregate>16U*1024U*1024U-value.payload.size())return false;
    aggregate+=value.payload.size();return true;
  };
  for(const auto& input:specification.inputs)if(!admit(input))return {Code::capacity,"experiment input byte bounds"};
  for(const auto& candidate:specification.candidates)if(!admit(candidate.change))return {Code::capacity,"experiment candidate byte bounds"};
  ExperimentSpecification checked;
  if(auto r=spec_read(spec_bytes(specification),checked);!r)return {Code::invalid,r.message};
  ScoreStrategyIdentity strategy_id;
  try{strategy_id=strategy.identity();}catch(...){return {Code::io_error,"score identity threw"};}
  if(!valid_token(strategy_id.configuration.schema)||strategy_id.configuration.payload.size()>65536)
    return {Code::capacity,"experiment score configuration bound"};
  ScoreStrategyIdentity validated;
  if(!strategy_read(strategy_bytes(strategy_id),validated))return {Code::invalid,"score identity is invalid"};
  RecordedExperiment frozen;ev::StoredRecord intent;auto found=read_intent(store_,checked.id,frozen,intent);
  if(found.code==Code::not_found){
    ev::StoredRecord engine_record;EngineIdentity identity;EngineState state;PreparedTransition transition;std::vector<Digest> refs;
    if(auto r=canonical_engine(store_,source.identity().instance_id,engine_record,identity,state,transition,refs);!r)return r;
    if(identity!=source.identity()||state!=source.state())return {Code::conflict,"trial source differs from its canonical checkpoint"};
    const auto intent_bytes=pack({"recorded-experiment/1",spec_bytes(checked),encode_checkpoint(identity,state),strategy_bytes(strategy_id)});
    if(auto r=publish(store_,intent_kind,checked.id,"experiment-intent-"+sha256(checked.id),intent_bytes,std::move(refs),
      {{"sdk.engine",identity.instance_id,engine_record.revision,engine_record.digest}});!r)return r;
    found=read_intent(store_,checked.id,frozen,intent);
  }
  if(!found)return found;
  if(frozen.source!=source.identity()||spec_bytes(frozen.specification)!=spec_bytes(checked)||strategy_bytes(frozen.strategy)!=strategy_bytes(strategy_id))
    return {Code::conflict,"experiment ID was bound to another source, program or strategy"};
  if(auto r=load(checked.id,frozen);!r)return r;
  if(frozen.completed){output=std::move(frozen);return {};}
  std::unique_ptr<IEngine> isolated;
  if(auto r=catalog_.create(frozen.source,frozen.source_state.configuration,&frozen.source_state,isolated);!r)return r;
  ExperimentRequest request{checked.id,frozen.source_state.revision,state_digest(frozen.source_state),checked.inputs,checked.candidates};
  ExperimentResult result;
  if(auto r=ExperimentService{}.select(*isolated,request,strategy,result);!r)return r;
  if(auto r=publish(store_,result_kind,checked.id,"experiment-result-"+sha256(checked.id),result_bytes(result),{intent.record.payload},
      {{std::string(intent_kind),checked.id,intent.revision,intent.digest}});!r)return r;
  frozen.result=std::move(result);frozen.completed=true;output=std::move(frozen);return {};
}
Result ExperimentHistoryService::apply_selected(std::string_view id,std::string operation,SessionService& runtime){
  if(!valid_token(operation))return {Code::invalid,"application operation ID"};
  RecordedExperiment frozen;if(auto r=load(id,frozen);!r)return r;
  if(!frozen.completed)return {Code::busy,"experiment selection is incomplete"};
  if(frozen.result.chosen_candidate.empty())return {Code::completed,"baseline was selected; no engine change is required"};
  const auto candidate=std::find_if(frozen.specification.candidates.begin(),frozen.specification.candidates.end(),
    [&](const auto& item){return item.id==frozen.result.chosen_candidate;});
  if(candidate==frozen.specification.candidates.end())return bad("chosen change is unavailable");
  StepRequest request{std::move(operation),frozen.source_state.revision,candidate->change};
  ev::CommitReceipt receipt;
  const auto previous=store_.lookup_operation(request.operation_id,receipt);
  if(previous)return receipt.request_digest==request_digest(frozen.source,request)?Result{}:Result{Code::conflict,"application operation ID was reused"};
  if(previous.code!=Code::not_found)return previous;
  auto* live=runtime.find(frozen.source.instance_id);
  if(!live||live->identity()!=frozen.source)return {Code::conflict,"selected source identity is no longer live"};
  return runtime.step(frozen.source.instance_id,request,true);
}
Result ExperimentHistoryService::review(std::string_view id,std::string operation,const IScoreStrategy& strategy,OutcomeReview& output){
  if(!valid_token(operation))return {Code::invalid,"outcome operation ID"};
  ev::StoredRecord previous;Bytes prior;const auto found=object(store_,outcome_kind,id,previous,prior);
  if(found)return review_read(prior,operation,output);
  if(found.code!=Code::not_found)return found;
  RecordedExperiment frozen;if(auto r=load(id,frozen);!r)return r;
  if(!frozen.completed)return {Code::busy,"experiment selection is incomplete"};
  ev::StoredRecord engine_record,result_record;EngineIdentity identity;EngineState state;PreparedTransition transition;
  std::vector<Digest> refs;
  if(auto r=experiment_evidence::canonical_engine(store_,frozen.source.instance_id,engine_record,identity,state,transition,refs);!r)return r;
  std::vector<ev::RecordGuard> guards{{"sdk.engine",identity.instance_id,engine_record.revision,engine_record.digest}};
  std::vector<Observation> observations;
  if(auto observed=experiment_evidence::observations(store_,identity,state,observations,guards);!observed)
    return observed;
  OutcomeReview result;
  if(auto r=ExperimentService{}.compare_outcome(frozen.result,{identity,state,std::move(observations),refs[1]},strategy,result);!r)return r;
  if(auto r=store_.latest(result_kind,id,result_record);!r)return r;
  ev::StoredRecord intent_record;
  if(auto r=store_.latest(intent_kind,id,intent_record);!r)return r;
  if(result_record.record.tombstone||intent_record.record.tombstone)return {Code::history_expired,"experiment was released during review"};
  guards.push_back({std::string(intent_kind),std::string(id),intent_record.revision,intent_record.digest});
  guards.push_back({std::string(result_kind),std::string(id),result_record.revision,result_record.digest});
  refs.push_back(result_record.record.payload);
  if(auto r=publish(store_,outcome_kind,id,operation,review_bytes(result,operation),std::move(refs),std::move(guards));!r)return r;
  output=std::move(result);return {};
}
Result ExperimentHistoryService::release(std::string_view id,std::string operation){
  if(!valid_token(id,80)||!valid_token(operation))return {Code::invalid,"experiment release identity"};
  const auto digest=sha256(pack({"release-experiment/1",std::string(id)}));
  ev::CommitReceipt previous;const auto prior=store_.lookup_operation(operation,previous);
  if(prior&&previous.request_digest!=digest)return {Code::conflict,"release operation ID was reused"};
  if(!prior&&prior.code!=Code::not_found)return prior;
  ev::CommitBatch batch;batch.operation_id=std::move(operation);batch.request_digest=digest;
  batch.commit_class=ev::CommitClass::maintenance;
  for(const auto kind:{intent_kind,result_kind,outcome_kind}){
    ev::StoredRecord record;const auto found=store_.latest(kind,id,record);
    if(found.code==Code::not_found)continue;
    if(!found)return found;
    batch.guards.push_back({std::string(kind),std::string(id),record.revision,record.digest});
    batch.records.push_back({std::string(kind),std::string(id),record.record.payload,{},true});
    batch.expire_blobs.push_back(record.record.payload);
  }
  if(batch.records.empty())return {Code::not_found,"experiment does not exist"};
  if(!prior){ev::CommitReceipt receipt;if(auto r=store_.commit(batch,receipt);!r)return r;}
  for(const auto& address:batch.expire_blobs){
    if(auto r=store_.reclaim_expired_blob(address,store_.generation());!r)
      return {Code::committed_degraded,"experiment roots released; retry payload reclamation: "+r.message};
  }
  return {};
}
}

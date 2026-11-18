#include "anaiero/sdk/experiment_history.hpp"
#include <algorithm>
#include <charconv>
namespace anaiero::sdk {
namespace {
namespace ev=evidence;
constexpr std::string_view intent_kind="sdk.experiment-intent",result_kind="sdk.experiment-result",outcome_kind="sdk.experiment-outcome";
Result bad(std::string message){return {Code::corrupt,std::move(message)};}
template<class Number> bool number(std::string_view text,Number& value){
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
  return !text.empty()&&parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&std::to_string(value)==text;
}
Bytes strategy_bytes(const ScoreStrategyIdentity& value){return pack({value.id,value.implementation_sha256,encode_envelope(value.configuration)});}
Result strategy_read(std::string_view bytes,ScoreStrategyIdentity& value){
  std::vector<std::string> f;
  if(!unpack(bytes,f,3)||f.size()!=3||!valid_token(f[0])||!valid_digest(f[1])||!decode_envelope(f[2],value.configuration))return bad("experiment strategy");
  value.id=f[0];value.implementation_sha256=f[1];return {};
}
Bytes spec_bytes(const ExperimentSpecification& value){
  std::vector<std::string> inputs,candidates;
  for(const auto& input:value.inputs)inputs.push_back(encode_envelope(input));
  for(const auto& candidate:value.candidates)candidates.push_back(pack({candidate.id,encode_envelope(candidate.change)}));
  return pack({"experiment-specification/1",value.id,pack(inputs),pack(candidates)});
}
Result spec_read(std::string_view bytes,ExperimentSpecification& value){
  std::vector<std::string> f,inputs,candidates;
  if(!unpack(bytes,f,4)||f.size()!=4||f[0]!="experiment-specification/1"||!valid_token(f[1],80)||
     !unpack(f[2],inputs,128)||inputs.empty()||!unpack(f[3],candidates,8)||candidates.empty())return bad("experiment specification bounds");
  value={};value.id=f[1];
  for(const auto& bytes_input:inputs){Envelope input;if(!decode_envelope(bytes_input,input))return bad("experiment input");value.inputs.push_back(std::move(input));}
  for(const auto& bytes_candidate:candidates){
    std::vector<std::string> row;TrialCandidate candidate;
    if(!unpack(bytes_candidate,row,2)||row.size()!=2||!valid_token(row[0],80)||row[0]=="baseline"||!decode_envelope(row[1],candidate.change))return bad("experiment candidate");
    if(std::any_of(value.candidates.begin(),value.candidates.end(),[&](const auto& old){return old.id==row[0];}))return bad("duplicate experiment candidate");
    candidate.id=row[0];value.candidates.push_back(std::move(candidate));
  }
  return {};
}
Bytes score_bytes(const Score& value){return pack({std::to_string(value.value),value.eligible?"1":"0",value.rationale});}
Result score_read(std::string_view bytes,Score& value){
  std::vector<std::string> f;
  if(!unpack(bytes,f,3,1024)||f.size()!=3||!number(f[0],value.value)||(f[1]!="0"&&f[1]!="1")||f[2].size()>512)return bad("experiment score");
  value.eligible=f[1]=="1";value.rationale=f[2];return {};
}
Bytes trial_bytes(const TrialResult& value){
  std::vector<std::string> observations;for(const auto& item:value.observations)observations.push_back(encode_observation(item));
  return pack({value.id,encode_state(value.final_state),pack(observations),value.receipt_tip});
}
Result trial_read(std::string_view bytes,TrialResult& value){
  std::vector<std::string> f,observations;
  if(!unpack(bytes,f,4)||f.size()!=4||!valid_token(f[0])||!decode_state(f[1],value.final_state)||
     !unpack(f[2],observations,4096)||!valid_digest(f[3]))return bad("recorded trial");
  value.id=f[0];value.receipt_tip=f[3];value.observations.clear();
  for(const auto& item:observations){Observation observation;if(!decode_observation(item,observation))return bad("recorded trial observation");value.observations.push_back(std::move(observation));}
  return {};
}
Bytes result_bytes(const ExperimentResult& value){
  constexpr std::size_t maximum=8U*1024U*1024U;
  const auto baseline=trial_bytes(value.baseline);
  if(baseline.empty()||baseline.size()>maximum)return {};
  std::size_t total=baseline.size()+4096;
  std::vector<std::string> candidates;
  for(const auto& candidate:value.candidates){
    const auto trial=candidate.status?trial_bytes(candidate.trial):Bytes{};
    if(candidate.status&&trial.empty())return {};
    auto row=pack({candidate.id,std::to_string(static_cast<unsigned>(candidate.status.code)),candidate.status.message,trial,score_bytes(candidate.score)});
    if(row.empty()||row.size()>maximum||total>maximum-row.size())return {};
    total+=row.size();candidates.push_back(std::move(row));
  }
  return pack({"experiment-result/1",value.program_sha256,baseline,score_bytes(value.baseline_score),pack(candidates),value.chosen_candidate});
}
Result result_read(std::string_view bytes,ExperimentResult& value){
  std::vector<std::string> f,candidates;
  if(!unpack(bytes,f,6)||f.size()!=6||f[0]!="experiment-result/1"||!valid_digest(f[1])||
     !trial_read(f[2],value.baseline)||!score_read(f[3],value.baseline_score)||!unpack(f[4],candidates,8))return bad("recorded experiment result");
  value.program_sha256=f[1];value.chosen_candidate=f[5];value.candidates.clear();
  bool chosen=value.chosen_candidate.empty();
  for(const auto& item:candidates){
    std::vector<std::string> row;CandidateResult candidate;unsigned code=0;
    if(!unpack(item,row,5)||row.size()!=5||!valid_token(row[0],80)||!number(row[1],code)||code>static_cast<unsigned>(Code::deadline)||
       row[2].size()>4096||!score_read(row[4],candidate.score))return bad("recorded candidate result");
    candidate.id=row[0];candidate.status={static_cast<Code>(code),row[2]};
    if(candidate.status&&!trial_read(row[3],candidate.trial))return bad("recorded successful candidate trial");
    if(candidate.id==value.chosen_candidate){if(!candidate.status||!candidate.score.eligible)return bad("selected candidate is ineligible");chosen=true;}
    value.candidates.push_back(std::move(candidate));
  }
  if(!chosen)return bad("selected candidate is absent");
  return {};
}
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
Bytes review_bytes(const OutcomeReview& value,std::string_view operation){
  return pack({"experiment-outcome/1",std::string(operation),score_bytes(value.measured_score),value.meets_selected_score?"1":"0",
    value.exact_selected_state?"1":"0",value.expected_state_sha256,value.observed_state_sha256,value.observed_receipt_tip});
}
Result review_read(std::string_view bytes,std::string_view operation,OutcomeReview& value){
  std::vector<std::string> f;
  if(!unpack(bytes,f,8)||f.size()!=8||f[0]!="experiment-outcome/1"||!score_read(f[2],value.measured_score)||
    (f[3]!="0"&&f[3]!="1")||(f[4]!="0"&&f[4]!="1")||!valid_digest(f[5])||!valid_digest(f[6])||!valid_digest(f[7]))return bad("recorded outcome");
  if(f[1]!=operation)return {Code::conflict,"experiment already has a measured outcome under another operation ID"};
  value.meets_selected_score=f[3]=="1";value.exact_selected_state=f[4]=="1";
  value.expected_state_sha256=f[5];value.observed_state_sha256=f[6];value.observed_receipt_tip=f[7];return {};
}
Result canonical_engine(ev::ICommitStore& store,std::string_view instance,ev::StoredRecord& record,
                        EngineIdentity& identity,EngineState& state,PreparedTransition& transition,std::vector<Digest>& references){
  if(auto r=store.latest("sdk.engine",instance,record);!r)return r;
  if(record.record.tombstone)return {Code::expired,"engine is retired"};
  std::vector<std::string> f;Bytes checkpoint,prepared;
  if(!unpack(record.record.payload,f,4)||f.size()!=4||f[0]!="session-engine/1"||!decode_identity(f[1],identity)||
     !valid_digest(f[2])||!valid_digest(f[3]))return bad("canonical engine record");
  if(auto r=store.get_blob(f[2],checkpoint);!r)return r;
  if(auto r=store.get_blob(f[3],prepared);!r)return r;
  EngineIdentity stored;
  if(!decode_checkpoint(checkpoint,stored,state)||stored!=identity||!decode_transition(prepared,transition)||
     transition.identity!=identity||transition.target!=state)return bad("canonical engine evidence mismatch");
  references={f[2],f[3]};return {};
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
  if(auto r=canonical_engine(store_,frozen.source.instance_id,engine_record,identity,state,transition,refs);!r)return r;
  std::vector<ev::RecordGuard> guards{{"sdk.engine",identity.instance_id,engine_record.revision,engine_record.digest}};
  std::vector<Observation> observations;
  const auto prefix=sha256(pack({identity.source_uid,std::to_string(identity.epoch)}))+"/";
  std::string after;bool more=true;
  while(more){
    std::vector<ev::StoredRecord> page;
    if(auto r=store_.scan_latest("sdk.metric",prefix,after,32,page,more);!r)return r;
    if(more&&page.empty())return bad("outcome metric index made no progress");
    for(const auto& record:page){
      after=record.record.key;
      if(record.record.tombstone)continue;
      std::vector<std::string> f;EngineIdentity metric_identity;Observation observation;std::uint64_t revision=0;
      if(!unpack(record.record.payload,f,4)||f.size()!=4||f[0]!="session-metric/1"||!decode_identity(f[1],metric_identity)||
         metric_identity!=identity||!decode_observation(f[2],observation)||!number(f[3],revision)||revision>state.revision||observation.tick>state.tick)
        return bad("canonical outcome metric disagrees with engine state");
      if(observations.size()==128)return {Code::capacity,"outcome metric bound"};
      observations.push_back(std::move(observation));guards.push_back({"sdk.metric",record.record.key,record.revision,record.digest});
    }
  }
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

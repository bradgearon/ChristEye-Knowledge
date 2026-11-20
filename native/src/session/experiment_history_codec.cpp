#include "experiment_history_codec.hpp"
#include <algorithm>
#include <charconv>
#include <utility>
namespace anaiero::sdk::history_codec {
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
Bytes encode_intent(const ExperimentSpecification& spec,const EngineIdentity& identity,
                    const EngineState& state,const ScoreStrategyIdentity& strategy){
  return pack({"recorded-experiment/1",spec_bytes(spec),encode_checkpoint(identity,state),strategy_bytes(strategy)});
}
Result read_intent(std::string_view bytes,std::string_view expected_id,RecordedExperiment& value){
  std::vector<std::string> fields;
  if(!unpack(bytes,fields,4)||fields.size()!=4||fields[0]!="recorded-experiment/1"||
     !spec_read(fields[1],value.specification)||value.specification.id!=expected_id||
     !decode_checkpoint(fields[2],value.source,value.source_state)||
     !strategy_read(fields[3],value.strategy))
    return bad("recorded experiment intent");
  return {};
}

}

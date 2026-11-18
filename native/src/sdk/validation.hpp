#pragma once
#include "anaiero/sdk/engine.hpp"
#include <algorithm>
#include <limits>

namespace anaiero::sdk::detail {
inline bool has_schema(const std::vector<std::string>& schemas,std::string_view schema) {
  return std::find(schemas.begin(),schemas.end(),schema)!=schemas.end();
}
inline Result validate_transition(const EngineIdentity& identity,const EngineState& before,
    const StepRequest& request,const PreparedTransition& prepared,const EngineDescriptor* descriptor=nullptr,bool change=false) {
  const Limits limits=descriptor?descriptor->limits:Limits{};
  if(prepared.identity!=identity||prepared.operation_id!=request.operation_id||
      prepared.request_sha256!=request_digest(identity,request)||
      prepared.expected_state_sha256!=state_digest(before)||prepared.expected_revision!=before.revision||
      request.expected_revision!=before.revision)
    return {Code::corrupt,"prepared transition changed its source or request witness"};
  if(before.revision==std::numeric_limits<std::uint64_t>::max()||
      prepared.target.revision!=before.revision+1||prepared.target.tick<before.tick||
      !valid_state(prepared.target,limits)||(!change&&prepared.target.configuration!=before.configuration)||
      (descriptor&&(!has_schema(descriptor->checkpoint_schemas,prepared.target.checkpoint.schema)||
        !has_schema(descriptor->configuration_schemas,prepared.target.configuration.schema))))
    return {Code::corrupt,"prepared state revision, clock or schema is invalid"};
  if(change&&before.phase==Phase::paused&&prepared.target.phase!=Phase::paused)
    return {Code::corrupt,"a model change cannot implicitly resume a paused engine"};
  if(prepared.observations.size()>limits.max_observations||prepared.receipts.size()>limits.max_receipts)
    return {Code::capacity,"prepared evidence exceeds engine bounds"};
  for(std::size_t i=0;i<prepared.observations.size();++i) {
    const auto& o=prepared.observations[i];
    if(!valid_token(o.schema)||!valid_token(o.metric)||!valid_token(o.quantity)||!valid_token(o.unit)||
        o.tick>prepared.target.tick||o.tick<before.tick||
        (descriptor&&!has_schema(descriptor->observation_schemas,o.schema)))
      return {Code::corrupt,"prepared observation schema or clock is invalid"};
    for(std::size_t j=0;j<i;++j)
      if(prepared.observations[j].metric==o.metric)
        return {Code::conflict,"one transition contains duplicate metric keys"};
  }
  for(const auto& receipt:prepared.receipts)
    if(!valid_token(receipt.schema)||receipt.payload.size()>limits.max_payload_bytes)
      return {Code::capacity,"prepared receipt schema or byte bound"};
  return {};
}
}

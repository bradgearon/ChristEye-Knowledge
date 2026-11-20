#include "anaiero/sdk/session_record_codec.hpp"
#include <charconv>
#include <utility>

namespace anaiero::sdk {
Bytes encode_session_engine_record(const SessionEngineRecord& r) {
  if(!valid_identity(r.identity)||!valid_digest(r.checkpoint_sha256)||
     !valid_digest(r.transition_sha256))return {};
  return pack({"session-engine/1",encode_identity(r.identity),r.checkpoint_sha256,r.transition_sha256});
}
Result decode_session_engine_record(std::string_view bytes,SessionEngineRecord& result) {
  std::vector<std::string> fields;
  if(auto parsed=unpack(bytes,fields,4,8192);!parsed)return parsed;
  SessionEngineRecord decoded;
  if(fields.size()!=4||fields[0]!="session-engine/1"||
     !decode_identity(fields[1],decoded.identity))return {Code::corrupt,"session engine record"};
  decoded.checkpoint_sha256=fields[2];decoded.transition_sha256=fields[3];
  if(encode_session_engine_record(decoded)!=bytes)return {Code::corrupt,"invalid canonical session engine record"};
  result=std::move(decoded);return {};
}
Bytes encode_session_metric_record(const SessionMetricRecord& r) {
  if(!valid_identity(r.identity)||!valid_token(r.observation.schema)||
     !valid_token(r.observation.metric)||!valid_token(r.observation.quantity)||
     !valid_token(r.observation.unit))return {};
  return pack({"session-metric/1",encode_identity(r.identity),encode_observation(r.observation),
    std::to_string(r.revision)});
}
Result decode_session_metric_record(std::string_view bytes,SessionMetricRecord& result) {
  std::vector<std::string> fields;
  if(auto parsed=unpack(bytes,fields,4,16384);!parsed)return parsed;
  SessionMetricRecord decoded;
  if(fields.size()!=4||fields[0]!="session-metric/1"||
     !decode_identity(fields[1],decoded.identity)||!decode_observation(fields[2],decoded.observation))
    return {Code::corrupt,"session metric record"};
  const auto parsed=std::from_chars(fields[3].data(),fields[3].data()+fields[3].size(),decoded.revision);
  if(parsed.ec!=std::errc{}||parsed.ptr!=fields[3].data()+fields[3].size()||
     encode_session_metric_record(decoded)!=bytes)return {Code::corrupt,"session metric revision"};
  result=std::move(decoded);return {};
}
} // namespace anaiero::sdk

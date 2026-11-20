#include "anaiero/sdk/engine.hpp"
#include "sha256_impl.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <limits>

namespace anaiero::sdk {
namespace {
void append_u64(Bytes& out,std::uint64_t n) { for(unsigned i=0;i<8;++i)out+=static_cast<char>((n>>(8*i))&255U); }
bool read_u64(std::string_view in,std::size_t& offset,std::uint64_t& n) {
  if(offset>in.size()||in.size()-offset<8)return false;
  n=0;for(unsigned i=0;i<8;++i)n|=std::uint64_t(static_cast<unsigned char>(in[offset++]))<<(8*i);
  return true;
}
template<class T> bool number(std::string_view in,T& value) {
  if(in.empty())return false;
  const auto r=std::from_chars(in.data(),in.data()+in.size(),value);
  return r.ec==std::errc{}&&r.ptr==in.data()+in.size()&&std::to_string(value)==in;
}
Result bad(std::string why) { return {Code::corrupt,std::move(why)}; }
}
Digest sha256(std::string_view bytes) { return sha_internal::sha256_hex(bytes); }
bool valid_digest(std::string_view v) noexcept {
  return v.size()==64&&std::all_of(v.begin(),v.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
bool valid_token(std::string_view v,std::size_t max) noexcept {
  if(v.empty()||v.size()>max)return false;
  return std::all_of(v.begin(),v.end(),[](char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||
    (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c=='/'||c==':'||c=='+';});
}
std::string_view code_name(Code c) noexcept {
  constexpr std::array<std::string_view,20> names{"ok","invalid","unsupported","conflict","capacity","not_found",
    "corrupt","io_error","committed_degraded","cancelled","expired","unauthorized","history_expired","busy","completed",
    "archive_required","archive_unavailable","rebootstrap_required","deadline","unknown"};
  const auto i=static_cast<std::size_t>(c);return names[i<19?i:19];
}
Bytes pack(std::span<const std::string> fields) {
  constexpr std::size_t max=64U*1024U*1024U;
  if(fields.size()>65536)return {};
  std::size_t size=12;
  for(const auto& f:fields){if(f.size()>max||size>max-8||f.size()>max-size-8)return {};size+=8+f.size();}
  Bytes out;out.reserve(size);out="ASK1";append_u64(out,static_cast<std::uint64_t>(fields.size()));
  for(const auto& f:fields){append_u64(out,static_cast<std::uint64_t>(f.size()));out+=f;}
  return out;
}
Result unpack(std::string_view bytes,std::vector<std::string>& fields,std::size_t max_fields,std::size_t max_bytes) {
  if(bytes.size()>max_bytes)return {Code::capacity,"record exceeds byte bound"};
  if(!bytes.starts_with("ASK1"))return bad("record magic");
  std::size_t pos=4;std::uint64_t count=0;
  if(!read_u64(bytes,pos,count)||count>max_fields||count>(bytes.size()-pos)/8)return bad("record field count");
  std::vector<std::string> next;next.reserve(static_cast<std::size_t>(count));
  for(std::uint64_t i=0;i<count;++i){std::uint64_t len=0;
    if(!read_u64(bytes,pos,len)||len>bytes.size()-pos)return bad("record field length");
    next.emplace_back(bytes.substr(pos,static_cast<std::size_t>(len)));pos+=static_cast<std::size_t>(len);}
  if(pos!=bytes.size())return bad("record trailing bytes");
  fields=std::move(next);return {};
}
Bytes encode_envelope(const Envelope& e) { const std::array<std::string,3> f{"envelope/1",e.schema,e.payload};return pack(f); }
Result decode_envelope(std::string_view b,Envelope& e) {
  std::vector<std::string> f;const auto r=unpack(b,f,3);if(!r)return r;
  if(f.size()!=3||f[0]!="envelope/1"||!valid_token(f[1]))return bad("envelope schema");
  e={std::move(f[1]),std::move(f[2])};return {};
}
bool valid_identity(const EngineIdentity& i) noexcept {
  return valid_token(i.instance_id)&&valid_token(i.type_id)&&valid_digest(i.module_sha256)&&valid_digest(i.model_sha256)&&
    valid_token(i.behavior_id)&&valid_token(i.source_uid)&&valid_token(i.clock_domain)&&i.epoch!=0;
}
bool valid_state(const EngineState& s,const Limits& l) noexcept {
  return static_cast<unsigned>(s.phase)<=static_cast<unsigned>(Phase::faulted)&&valid_token(s.checkpoint.schema)&&
    valid_token(s.configuration.schema)&&s.checkpoint.payload.size()<=l.max_checkpoint_bytes&&
    s.configuration.payload.size()<=l.max_payload_bytes;
}
Bytes encode_identity(const EngineIdentity& i) {
  const std::array<std::string,10> f{"engine-identity/1",i.instance_id,i.type_id,i.module_sha256,i.model_sha256,
    i.behavior_id,i.source_uid,i.clock_domain,std::to_string(i.epoch),std::to_string(i.tick_ns)};return pack(f);
}
Result decode_identity(std::string_view b,EngineIdentity& i) {
  std::vector<std::string> f;const auto r=unpack(b,f,10,4096);if(!r)return r;
  EngineIdentity n;if(f.size()!=10||f[0]!="engine-identity/1"||!number(f[8],n.epoch)||!number(f[9],n.tick_ns))return bad("engine identity");
  n.instance_id=f[1];n.type_id=f[2];n.module_sha256=f[3];n.model_sha256=f[4];n.behavior_id=f[5];n.source_uid=f[6];n.clock_domain=f[7];
  if(!valid_identity(n))return bad("engine identity bounds");
  i=std::move(n);return {};
}
Bytes encode_state(const EngineState& s) {
  const std::array<std::string,6> f{"engine-state/1",std::to_string(s.revision),std::to_string(s.tick),
    std::to_string(static_cast<unsigned>(s.phase)),encode_envelope(s.checkpoint),encode_envelope(s.configuration)};return pack(f);
}
Result decode_state(std::string_view b,EngineState& s) {
  std::vector<std::string> f;const auto r=unpack(b,f,6);if(!r)return r;
  EngineState n;unsigned phase=0;
  if(f.size()!=6||f[0]!="engine-state/1"||!number(f[1],n.revision)||!number(f[2],n.tick)||!number(f[3],phase)||
    phase>static_cast<unsigned>(Phase::faulted))return bad("engine state");
  n.phase=static_cast<Phase>(phase);
  if(!decode_envelope(f[4],n.checkpoint)||!decode_envelope(f[5],n.configuration)||!valid_state(n))return bad("engine state envelope");
  s=std::move(n);return {};
}
Bytes encode_observation(const Observation& o) {
  const std::array<std::string,7> f{"observation/1",o.schema,o.metric,o.quantity,o.unit,std::to_string(o.value),std::to_string(o.tick)};return pack(f);
}
Result decode_observation(std::string_view b,Observation& o) {
  std::vector<std::string> f;const auto r=unpack(b,f,7,4096);if(!r)return r;
  Observation n;
  if(f.size()!=7||f[0]!="observation/1"||!number(f[5],n.value)||!number(f[6],n.tick))return bad("observation");
  n.schema=f[1];n.metric=f[2];n.quantity=f[3];n.unit=f[4];
  if(!valid_token(n.schema)||!valid_token(n.metric)||!valid_token(n.quantity)||!valid_token(n.unit))return bad("observation identity");
  o=std::move(n);return {};
}
Bytes encode_transition(const PreparedTransition& t) {
  std::vector<std::string> f{"prepared-transition/1",encode_identity(t.identity),t.operation_id,t.request_sha256,
    t.expected_state_sha256,std::to_string(t.expected_revision),encode_state(t.target),std::to_string(t.observations.size())};
  for(const auto& o:t.observations)f.push_back(encode_observation(o));
  f.push_back(std::to_string(t.receipts.size()));for(const auto& r:t.receipts)f.push_back(encode_envelope(r));return pack(f);
}
Result decode_transition(std::string_view b,PreparedTransition& t) {
  std::vector<std::string> f;const auto r=unpack(b,f,512);if(!r)return r;
  PreparedTransition n;std::size_t observations=0,receipts=0;
  if(f.size()<9||f[0]!="prepared-transition/1"||!decode_identity(f[1],n.identity)||!valid_token(f[2])||
    !valid_digest(f[3])||!valid_digest(f[4])||!number(f[5],n.expected_revision)||!decode_state(f[6],n.target)||
    !number(f[7],observations)||observations>128||observations>f.size()-9)return bad("transition header");
  n.operation_id=f[2];n.request_sha256=f[3];n.expected_state_sha256=f[4];
  for(std::size_t i=0;i<observations;++i){Observation o;if(!decode_observation(f[8+i],o))return bad("transition observation");n.observations.push_back(std::move(o));}
  const auto at=8+observations;
  if(!number(f[at],receipts)||receipts>128||f.size()!=at+1+receipts)return bad("transition receipt count");
  for(std::size_t i=0;i<receipts;++i){Envelope e;if(!decode_envelope(f[at+1+i],e))return bad("transition receipt");n.receipts.push_back(std::move(e));}
  t=std::move(n);return {};
}
Digest state_digest(const EngineState& s) { return sha256(encode_state(s)); }
Bytes encode_checkpoint(const EngineIdentity& identity,const EngineState& state) {
  return pack({"engine-checkpoint/1",encode_identity(identity),encode_state(state)});
}
Result decode_checkpoint(std::string_view bytes,EngineIdentity& identity,EngineState& state) {
  std::vector<std::string> fields;auto result=unpack(bytes,fields,3);if(!result)return result;
  EngineIdentity next_identity;EngineState next_state;
  if(fields.size()!=3||fields[0]!="engine-checkpoint/1"||!decode_identity(fields[1],next_identity)||
      !decode_state(fields[2],next_state))return bad("source-scoped checkpoint");
  identity=std::move(next_identity);state=std::move(next_state);return {};
}
Digest checkpoint_digest(const EngineIdentity& identity,const EngineState& state) {return sha256(encode_checkpoint(identity,state));}
Digest request_digest(const EngineIdentity& i,const StepRequest& r) {
  const std::array<std::string,5> f{"engine-request/1",encode_identity(i),r.operation_id,std::to_string(r.expected_revision),encode_envelope(r.input)};
  return sha256(pack(f));
}
Bytes encode_request(const StepRequest& request) {
  return pack({"step-request/1",request.operation_id,std::to_string(request.expected_revision),encode_envelope(request.input)});
}
Result decode_request(std::string_view bytes,StepRequest& request) {
  std::vector<std::string> fields;auto result=unpack(bytes,fields,4);if(!result)return result;
  StepRequest next;
  if(fields.size()!=4||fields[0]!="step-request/1"||!valid_token(fields[1])||
      !number(fields[2],next.expected_revision)||!decode_envelope(fields[3],next.input))return bad("step request");
  next.operation_id=std::move(fields[1]);request=std::move(next);return {};
}
Bytes encode_descriptor(const EngineDescriptor& descriptor) {
  return pack({"engine-descriptor/1",descriptor.type_id,descriptor.display_name,descriptor.module_sha256,
    descriptor.behavior_id,pack(descriptor.configuration_schemas),pack(descriptor.input_schemas),
    pack(descriptor.checkpoint_schemas),pack(descriptor.observation_schemas),pack(descriptor.change_schemas),
    descriptor.finite?"1":"0",descriptor.isolated_trials?"1":"0",descriptor.change_preparation?"1":"0",
    std::to_string(descriptor.limits.max_payload_bytes),std::to_string(descriptor.limits.max_checkpoint_bytes),
    std::to_string(descriptor.limits.max_observations),std::to_string(descriptor.limits.max_receipts)});
}
Result decode_descriptor(std::string_view bytes,EngineDescriptor& descriptor) {
  std::vector<std::string> fields;auto result=unpack(bytes,fields,17,16384);if(!result)return result;
  EngineDescriptor next;
  if(fields.size()!=17||fields[0]!="engine-descriptor/1"||!valid_token(fields[1])||
      fields[2].empty()||fields[2].size()>128||!valid_digest(fields[3])||!valid_token(fields[4]))return bad("module descriptor identity");
  next.type_id=fields[1];next.display_name=fields[2];next.module_sha256=fields[3];next.behavior_id=fields[4];
  std::vector<std::string>* lists[]{&next.configuration_schemas,&next.input_schemas,&next.checkpoint_schemas,
    &next.observation_schemas,&next.change_schemas};
  for(std::size_t i=0;i<5;++i) {
    if(!unpack(fields[5+i],*lists[i],16,4096))return bad("module descriptor schemas");
    for(const auto& schema:*lists[i])if(!valid_token(schema))return bad("module descriptor schema name");
  }
  for(std::size_t i=10;i<13;++i)if(fields[i]!="0"&&fields[i]!="1")return bad("module descriptor capability");
  next.finite=fields[10]=="1";next.isolated_trials=fields[11]=="1";next.change_preparation=fields[12]=="1";
  if(!number(fields[13],next.limits.max_payload_bytes)||!number(fields[14],next.limits.max_checkpoint_bytes)||
      !number(fields[15],next.limits.max_observations)||!number(fields[16],next.limits.max_receipts)||
      !next.limits.max_payload_bytes||next.limits.max_payload_bytes>4U*1024U*1024U||
      !next.limits.max_checkpoint_bytes||next.limits.max_checkpoint_bytes>4U*1024U*1024U||
      next.limits.max_observations>128||next.limits.max_receipts>128)return bad("module descriptor limits");
  descriptor=std::move(next);return {};
}
std::string_view phase_name(Phase p) noexcept {
  constexpr std::array<std::string_view,6> n{"ready","running","paused","completed","cancelled","faulted"};
  const auto i=static_cast<std::size_t>(p);return i<n.size()?n[i]:"invalid";
}
}

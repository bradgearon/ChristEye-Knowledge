#include "mesh_record_codec.hpp"
#include <charconv>
#include <set>
namespace anaiero::sdk::mesh::recordcodec {
namespace ev=evidence;
namespace lc=lifecycle;
namespace {
Result fail(Code code,std::string message){return {code,std::move(message)};}
std::string number(std::uint64_t value){return std::to_string(value);}
bool integer(std::string_view text,std::uint64_t& value){
  if(text.empty())return false;
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
  return parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&number(value)==text;
}
}
lc::SourceRef scope(const EngineIdentity& id){return {id.source_uid,id.epoch};}
Bytes encode_scope(const lc::SourceRef& value){return pack({value.uid,number(value.epoch)});}
Bytes encode_cursor(const lc::Cursor& value){return pack({number(value.sequence),value.payload_sha256});}
bool decode_cursor(std::string_view bytes,lc::Cursor& value){
  std::vector<std::string> f;if(!unpack(bytes,f,2,256)||f.size()!=2||!integer(f[0],value.sequence))return false;
  value.payload_sha256=f[1];return value.sequence?valid_digest(value.payload_sha256):value.payload_sha256.empty();
}
std::string grant_key(std::string_view principal,const lc::SourceRef& source){return sha256(pack({std::string(principal),encode_scope(source)}));}
std::string receive_key(std::string_view principal,std::string_view subscription,const lc::SourceRef& source){
  return sha256(pack({std::string(principal),std::string(subscription),encode_scope(source)}));
}
std::string received_observation_key(std::string_view principal,const lc::SourceRef& source,std::uint64_t sequence){
  const auto digits=number(sequence);return grant_key(principal,source)+":"+std::string(20-digits.size(),'0')+digits;
}
std::string operation(std::string_view action,std::string_view bytes){return "mesh2-"+std::string(action)+"-"+sha256(bytes);}
Bytes offer_metadata(const BootstrapOffer& offer){return pack({"mesh-bootstrap-offer/2",offer.id,offer.subscription_id,
  number(offer.generation),number(offer.lease_until_unix_s),encode_identity(offer.source),encode_cursor(offer.tip),offer.checkpoint_sha256});}
Result decode_receive(const ev::StoredRecord& record,ReceiveReceipt& out){
  std::vector<std::string> f;
  if(!unpack(record.record.payload,f,6,4096)||f.size()!=6||f[0]!="mesh-receive/2"||
      !integer(f[1],out.generation)||!decode_cursor(f[2],out.cursor)||!valid_digest(f[3]))
    return fail(Code::corrupt,"invalid canonical mesh receive cursor");
  out.receipt_sha256=f[3];out.store_generation=record.generation;return {};
}
Bytes encode_retention_request(const ReceiverRetentionRequest& value){
  return pack({"mesh-receiver-retention/1",value.id,value.principal,value.source.uid,number(value.source.epoch),
    number(value.first_sequence),number(value.last_sequence),pack(value.artifacts),pack(value.bootstrap_ids),pack(value.delivery_receipts)});
}
bool valid_retention_request(const ReceiverRetentionRequest& value){
  if(!valid_token(value.id,256)||!valid_token(value.principal,256)||!valid_token(value.source.uid,256)||!value.source.epoch||
     value.artifacts.size()>8||value.bootstrap_ids.size()>8||value.delivery_receipts.size()>8)return false;
  if(value.first_sequence||value.last_sequence){
    if(!value.first_sequence||value.last_sequence<value.first_sequence||value.last_sequence-value.first_sequence>=32)return false;
  }else if(value.artifacts.empty()&&value.bootstrap_ids.empty()&&value.delivery_receipts.empty())return false;
  std::set<std::string> unique;
  for(const auto& id:value.artifacts)if(!valid_digest(id)||!unique.insert(id).second)return false;
  unique.clear();for(const auto& id:value.bootstrap_ids)if(!valid_token(id,256)||!unique.insert(id).second)return false;
  unique.clear();for(const auto& id:value.delivery_receipts)if(!valid_digest(id)||!unique.insert(id).second)return false;
  return true;
}
Bytes encode_retention_plan(const ReceiverRetentionPlan& value){
  return pack({"mesh-receiver-retention-plan/1",encode_retention_request(value.request),number(static_cast<std::uint64_t>(value.phase)),
    value.store_id,value.reviewed_root,value.candidate_sha256,number(value.reviewed_generation),number(value.expected_bytes),pack(value.objects)});
}
Result decode_retention_plan(std::string_view bytes,ReceiverRetentionPlan& out){
  out={};std::vector<std::string> f,r;std::uint64_t phase=0;
  if(!unpack(bytes,f,9,32768)||f.size()!=9||f[0]!="mesh-receiver-retention-plan/1"||
     !unpack(f[1],r,10,16384)||r.size()!=10||r[0]!="mesh-receiver-retention/1"||
     !integer(r[4],out.request.source.epoch)||!integer(r[5],out.request.first_sequence)||!integer(r[6],out.request.last_sequence)||
     !unpack(r[7],out.request.artifacts,8,4096)||!unpack(r[8],out.request.bootstrap_ids,8,4096)||!unpack(r[9],out.request.delivery_receipts,8,4096)||
     !integer(f[2],phase)||phase>2||!valid_digest(f[3])||!valid_digest(f[4])||!valid_digest(f[5])||
     !integer(f[6],out.reviewed_generation)||!out.reviewed_generation||!integer(f[7],out.expected_bytes)||
     !unpack(f[8],out.objects,64,8192)||out.objects.empty())return fail(Code::corrupt,"invalid receiver retention plan");
  out.request.id=r[1];out.request.principal=r[2];out.request.source.uid=r[3];
  if(!valid_retention_request(out.request))return fail(Code::corrupt,"invalid receiver retention scope");
  std::set<Digest> unique;for(const auto& object:out.objects)if(!valid_digest(object)||!unique.insert(object).second)return fail(Code::corrupt,"invalid receiver retention objects");
  out.phase=static_cast<lc::MaintenancePhase>(phase);out.store_id=f[3];out.reviewed_root=f[4];out.candidate_sha256=f[5];return {};
}
}

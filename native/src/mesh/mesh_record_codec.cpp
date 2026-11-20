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

namespace anaiero::sdk::mesh {
namespace lc=lifecycle;
namespace {
Result fail(Code code,std::string text){return {code,std::move(text)};}
std::string number(std::uint64_t value){return std::to_string(value);}
bool integer(std::string_view text,std::uint64_t& value){
  if(text.empty())return false;
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
  return parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&number(value)==text;
}
}
using recordcodec::encode_cursor;using recordcodec::decode_cursor;
using recordcodec::scope;
Bytes encode_grant(const Grant& grant){
  std::vector<std::string> f{"mesh-grant/2",grant.principal,encode_identity(grant.source),number(grant.generation),
    grant.send?"1":"0",grant.receive?"1":"0",grant.bootstrap?"1":"0"};
  f.insert(f.end(),grant.artifacts.begin(),grant.artifacts.end());return pack(f);
}
Result decode_grant(std::string_view bytes,Grant& grant){
  std::vector<std::string> f;if(!unpack(bytes,f,39,32768)||f.size()<7||f[0]!="mesh-grant/2"||
      !valid_token(f[1],256)||!decode_identity(f[2],grant.source)||!integer(f[3],grant.generation)||!grant.generation)
    return fail(Code::corrupt,"invalid mesh scope grant");
  for(std::size_t i=4;i<7;++i)if(f[i]!="0"&&f[i]!="1")return fail(Code::corrupt,"invalid mesh grant flag");
  grant.principal=f[1];grant.send=f[4]=="1";grant.receive=f[5]=="1";grant.bootstrap=f[6]=="1";
  grant.artifacts.assign(f.begin()+7,f.end());std::set<Digest> unique;
  for(const auto& digest:grant.artifacts)if(!valid_digest(digest)||!unique.insert(digest).second)
    return fail(Code::corrupt,"invalid mesh artifact grant");
  return {};
}
Bytes encode_delivery(const Delivery& page){
  std::vector<std::string> f{"mesh-page/2",page.subscription_id,number(page.generation),encode_identity(page.source),
    encode_cursor(page.from),encode_cursor(page.to)};
  for(const auto& item:page.observations)f.push_back(pack({number(item.claim.sequence),item.claim.payload_digest,
    item.claim.receipt_digest,item.payload}));
  return pack(f);
}
Result decode_delivery(std::string_view bytes,Delivery& page,const Options& options){
  page={};std::vector<std::string> f;
  if(!unpack(bytes,f,6+options.maximum_page_observations,options.maximum_page_bytes)||f.size()<7||f[0]!="mesh-page/2"||
      !valid_token(f[1],256)||!integer(f[2],page.generation)||!page.generation||!decode_identity(f[3],page.source)||
      !decode_cursor(f[4],page.from)||!decode_cursor(f[5],page.to))return fail(Code::invalid,"invalid bounded mesh page");
  page.subscription_id=f[1];auto sequence=page.from.sequence;
  for(std::size_t i=6;i<f.size();++i){
    std::vector<std::string> item;DeliveryItem value;lc::SourceRef source;std::uint64_t observed_sequence=0;Observation observation;
    if(!unpack(f[i],item,4,options.maximum_page_bytes)||item.size()!=4||!integer(item[0],value.claim.sequence)||
       sequence==std::numeric_limits<std::uint64_t>::max()||value.claim.sequence!=++sequence||
       !valid_digest(item[1])||!valid_digest(item[2])||sha256(item[3])!=item[1]||
       !lc::decode_source_observation(item[3],source,observed_sequence,observation)||source!=scope(page.source)||observed_sequence!=sequence)
      return fail(Code::corrupt,"mesh page observation identity, digest or continuity mismatch");
    value.claim={page.source.source_uid,page.source.epoch,sequence,item[1],item[2]};value.payload=std::move(item[3]);
    page.observations.push_back(std::move(value));
  }
  if(page.to!=lc::Cursor{sequence,page.observations.back().claim.payload_digest})
    return fail(Code::corrupt,"mesh page terminal cursor differs from its bytes");
  return {};
}
Bytes encode_bootstrap(const BootstrapOffer& offer){return pack({"mesh-bootstrap/2",offer.id,offer.subscription_id,
  number(offer.generation),number(offer.lease_until_unix_s),encode_identity(offer.source),encode_cursor(offer.tip),
  offer.checkpoint_sha256,offer.checkpoint});}
Result decode_bootstrap(std::string_view bytes,BootstrapOffer& offer,std::size_t maximum){
  std::vector<std::string> f;
  if(!unpack(bytes,f,9,maximum)||f.size()!=9||f[0]!="mesh-bootstrap/2"||!valid_token(f[1],256)||!valid_token(f[2],256)||
      !integer(f[3],offer.generation)||!offer.generation||!integer(f[4],offer.lease_until_unix_s)||
      !decode_identity(f[5],offer.source)||!decode_cursor(f[6],offer.tip)||!valid_digest(f[7])||sha256(f[8])!=f[7])
    return fail(Code::invalid,"invalid bounded mesh bootstrap");
  EngineIdentity id;EngineState state;if(!decode_checkpoint(f[8],id,state)||id!=offer.source)
    return fail(Code::corrupt,"bootstrap checkpoint context mismatch");
  offer.id=f[1];offer.subscription_id=f[2];offer.checkpoint_sha256=f[7];offer.checkpoint=std::move(f[8]);return {};
}
}

#pragma once
#include "anaiero/contracts/mesh_ingress.hpp"
#include "anaiero/infrastructure/windstore_mesh_replay.hpp"
#include "christeye/living/durable_registry.hpp"
#include <sstream>
#include <string>
namespace anaiero::mesh {
class WindStoreMeshIngressStore final:public IMeshIngressStore,public IMeshReceiptVerifier {
  using Registry=christeye::living::durable::Registry;
  Registry& metadata_;
  WindStoreMeshReplayStore& evidence_;
  static std::string body(const IngressReceipt& value){
    const auto& scope=value.scope;
    return "CEVMESHRECEIPT/1\npeer "+value.peer.principal+"\ncertificate "+value.certificate_sha256+
      "\ngeneration "+std::to_string(value.peer.generation)+"\npolicy "+std::to_string(value.policy_revision)+
      "\nreceived "+std::to_string(value.received_unix_s)+"\nkind "+std::to_string(static_cast<unsigned>(value.kind))+"\nsource "+std::to_string(scope.source)+' '+
      std::to_string(scope.epoch)+' '+std::to_string(scope.clock)+' '+std::to_string(scope.colony)+' '+
      std::to_string(value.source_sequence)+"\npayload "+value.payload_sha256+'\n';
  }
  static bool valid(const IngressReceipt& value)noexcept {
    return mesh_token(value.peer.principal)&&value.peer.generation&&mesh_digest(value.certificate_sha256)&&
      mesh_digest(value.payload_sha256)&&value.policy_revision&&value.received_unix_s&&
      value.source_sequence&&valid_scope(value.scope)&&
      (value.kind==IngressKind::mutual_tls||value.kind==IngressKind::local_import);
  }
  static std::string encode(const IngressReceipt& value){
    const auto bytes=body(value);return bytes+"seal "+christeye::living::plans::sha256_hex(bytes)+'\n';
  }
  static bool decode(std::string_view bytes,IngressReceipt& result){
    std::istringstream in{std::string(bytes)};IngressReceipt value;std::string magic,key,hash;unsigned kind=0;
    if(!(in>>magic)||magic!="CEVMESHRECEIPT/1"||!(in>>key>>value.peer.principal)||key!="peer"||
      !(in>>key>>value.certificate_sha256)||key!="certificate"||!(in>>key>>value.peer.generation)||key!="generation"||
      !(in>>key>>value.policy_revision)||key!="policy"||!(in>>key>>value.received_unix_s)||key!="received"||
      !(in>>key>>kind)||key!="kind"||(kind!=1&&kind!=2)||
      !(in>>key>>value.scope.source>>value.scope.epoch>>value.scope.clock>>value.scope.colony>>value.source_sequence)||key!="source"||
      !(in>>key>>value.payload_sha256)||key!="payload"||!(in>>key>>hash)||key!="seal"||
      (in>>key))return false;
    value.kind=static_cast<IngressKind>(kind);
    if(!valid(value)||encode(value)!=bytes)return false;
    value.receipt_sha256=christeye::living::plans::sha256_hex(bytes);result=std::move(value);return true;
  }
public:
  WindStoreMeshIngressStore(Registry& metadata,WindStoreMeshReplayStore& evidence)noexcept:
    metadata_(metadata),evidence_(evidence){}
  ReplayCode check(const runtime::Observation& observation,bool& duplicate)noexcept override {
    return evidence_.check_append(observation,duplicate);
  }
  ReplayCode verify_committed(const runtime::Observation& observation)const noexcept override {
    namespace db=christeye::living::durable;
    if(!runtime::valid_observation(observation)||!observation.source_epoch)return ReplayCode::invalid;
    runtime::EpochObservationCodec codec;std::array<std::byte,runtime::kEpochObservationFrame> frame{};
    if(!codec.encode(observation,frame))return ReplayCode::invalid;
    const auto digest=christeye::living::plans::sha256_hex(
      std::string_view(reinterpret_cast<const char*>(frame.data()),frame.size()));
    const auto entity="mesh-ingress-"+std::to_string(observation.source);
    const auto clock="epoch-"+std::to_string(observation.source_epoch);
    db::Event event;bool found=false;
    if(!metadata_.find_event("MESH_INGRESS_RECEIPT",entity,clock,"anaiero-mesh",observation.sequence,event,found)||!found)
      return ReplayCode::corrupt;
    IngressReceipt receipt;std::string bytes;
    if(event.origin!="SIMULATED"||!metadata_.get(event.object_hash,bytes)||!decode(bytes,receipt)||
      receipt.receipt_sha256!=event.object_hash||receipt.source_sequence!=observation.sequence||
      receipt.scope!=Scope{observation.source,observation.source_epoch,observation.clock_domain,observation.colony}||
      receipt.payload_sha256!=digest)return ReplayCode::corrupt;
    return ReplayCode::ok;
  }
  ReplayCode prepare(IngressReceipt& receipt,bool evidence_exists)noexcept override {
    namespace db=christeye::living::durable;
    if(!valid(receipt))return ReplayCode::invalid;
    const auto entity="mesh-ingress-"+std::to_string(receipt.scope.source);
    const auto clock="epoch-"+std::to_string(receipt.scope.epoch);
    db::Event event;bool found=false;
    if(!metadata_.find_event("MESH_INGRESS_RECEIPT",entity,clock,"anaiero-mesh",
      receipt.source_sequence,event,found))return ReplayCode::corrupt;
    if(found){
      IngressReceipt prior;std::string bytes;
      if(!metadata_.get(event.object_hash,bytes)||!decode(bytes,prior)||prior.receipt_sha256!=event.object_hash)
        return ReplayCode::corrupt;
      if(prior.peer.principal!=receipt.peer.principal||prior.scope!=receipt.scope||
        prior.source_sequence!=receipt.source_sequence||prior.payload_sha256!=receipt.payload_sha256)return ReplayCode::conflict;
      receipt=std::move(prior);return ReplayCode::ok;
    }
    if(evidence_exists)return ReplayCode::corrupt;
    // Source ownership survives certificate rotation, policy replacement and
    // revocation. A different principal must begin a new source epoch rather
    // than continue another principal's established observation sequence.
    db::Event latest;bool has_latest=false;
    if(!metadata_.latest_event("MESH_INGRESS_RECEIPT",entity,clock,latest,has_latest,"anaiero-mesh"))return ReplayCode::corrupt;
    if(has_latest){
      IngressReceipt owner;std::string bytes;
      if(!metadata_.get(latest.object_hash,bytes)||!decode(bytes,owner)||owner.receipt_sha256!=latest.object_hash||
        owner.source_sequence!=latest.source_tick||owner.scope.source!=receipt.scope.source||owner.scope.epoch!=receipt.scope.epoch)
        return ReplayCode::corrupt;
      if(owner.peer.principal!=receipt.peer.principal||owner.scope!=receipt.scope)return ReplayCode::conflict;
    }
    const auto bytes=encode(receipt);const auto object=metadata_.put(bytes);
    if(object.code!=db::Code::ok)return ReplayCode::persistence_failed;
    const auto published=metadata_.publish("MESH_INGRESS_RECEIPT",entity,object.hash,"SIMULATED",
      receipt.source_sequence,clock,"anaiero-mesh");
    if(published.code!=db::Code::ok)return ReplayCode::persistence_failed;
    receipt.receipt_sha256=object.hash;return ReplayCode::ok;
  }
  ReplayCode commit(const runtime::Observation& observation,ReplayCursor& cursor)noexcept override {
    return evidence_.append(observation,&cursor);
  }
};
} // namespace anaiero::mesh

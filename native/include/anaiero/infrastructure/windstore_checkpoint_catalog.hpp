#pragma once
// Content-addressed checkpoint manifest and payload under existing WindStore.
// No destructive journal compaction and no bypass of CAS verification.
#include "anaiero/contracts/checkpoint_catalog.hpp"
#include "christeye/living/durable_registry.hpp"
#include "christeye/living/plan_hash.hpp"
#include <array>
#include <charconv>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
namespace anaiero::engine {
class WindStoreCheckpointCatalog final:public ICheckpointCatalog {
  christeye::living::durable::Registry& db_;
  using Registry=christeye::living::durable::Registry;
  static std::string entity(std::uint32_t id){return "engine-"+std::to_string(id);}
  static std::string clock(std::uint64_t epoch){return "epoch-"+std::to_string(epoch);}
  static std::string producer(std::uint32_t source){return "source-"+std::to_string(source);}
  static std::string wire(const CheckpointRecord& r) {
    return "CEVENGINESNAPSHOT/1\nengine "+std::to_string(r.engine)+
      "\nsource "+std::to_string(r.source)+
      "\nepoch "+std::to_string(r.epoch)+
      "\nrevision "+std::to_string(r.revision)+
      "\nmodel "+r.model_sha256+"\npayload "+r.checkpoint_sha256+"\nend\n";
  }
  static bool number(std::string_view s,std::uint64_t& out) noexcept {
    const auto [last,ec]=std::from_chars(s.data(),s.data()+s.size(),out);
    return ec==std::errc{}&&last==s.data()+s.size();
  }
public:
  explicit WindStoreCheckpointCatalog(Registry& db)noexcept:db_(db){}
  bool commit(const CheckpointRecord& r,std::string_view checkpoint) noexcept override {
    using Code=christeye::living::durable::Code;
    if(!db_.is_open()||checkpoint.empty()||
       christeye::living::plans::sha256_hex(checkpoint)!=r.checkpoint_sha256)return false;
    const auto name=entity(r.engine),tag=clock(r.epoch);
    christeye::living::durable::Event previous;bool found=false;
    if(!db_.latest_event("CULTURE_CHECKPOINT",name,tag,previous,found)||
       (found&&r.revision<=previous.source_tick))return false; // immutable monotonic version
    const auto payload=db_.put(checkpoint);
    if(payload.code!=Code::ok||payload.hash!=r.checkpoint_sha256)return false;
    const std::array<std::string,1> references{r.checkpoint_sha256};
    const auto manifest=db_.put_referenced(wire(r),references);
    if(manifest.code!=Code::ok)return false;
    return db_.publish("CULTURE_CHECKPOINT",name,manifest.hash,
       "SIMULATED",r.revision,tag,producer(r.source)).code==Code::ok;
  }
  bool latest(std::uint32_t id,std::uint64_t epoch,CheckpointRecord& out,
              std::string& checkpoint) noexcept override {
    const auto name=entity(id),tag=clock(epoch);
    christeye::living::durable::Event event;bool found=false;
    if(!db_.latest_event("CULTURE_CHECKPOINT",name,tag,event,found)||!found)return false;
    const auto* it=&event;
    {
      std::string raw;
      if(!db_.get(it->object_hash,raw))return false;
      std::istringstream input(raw);
      std::string line;
      auto field=[&](std::string_view name,std::string& result)->bool {
        if(!std::getline(input,line)||!line.starts_with(name))return false;
        result=line.substr(name.size());return true;
      };
      if(!std::getline(input,line)||line!="CEVENGINESNAPSHOT/1")return false;
      std::string a,b,c,d,model,payload;
      if(!field("engine ",a)||!field("source ",b)||!field("epoch ",c)||
         !field("revision ",d)||!field("model ",model)||
         !field("payload ",payload)||!std::getline(input,line)||line!="end"||
         std::getline(input,line))return false;
      std::uint64_t n[4]{};
      if(!number(a,n[0])||!number(b,n[1])||!number(c,n[2])||!number(d,n[3])||
         n[0]!=id||n[1]>UINT32_MAX||n[2]!=epoch||
         n[3]!=it->source_tick||it->producer!=producer(static_cast<std::uint32_t>(n[1]))||
         payload.size()!=64||model.size()!=64)return false;
      if(!db_.get(payload,checkpoint)||
         christeye::living::plans::sha256_hex(checkpoint)!=payload)return false;
      out={id,static_cast<std::uint32_t>(n[1]),epoch,n[3],model,payload};
      return true;
    }
  }
};
} // namespace anaiero::engine

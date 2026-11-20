#include "source_authority_evidence.hpp"
#include <algorithm>
namespace anaiero::sdk::lifecycle {
namespace {
constexpr std::string_view kind="lifecycle.ownership";
Result fail(Code code,std::string text){return {code,std::move(text)};}
Result inspect(evidence::ICommitStore& store,std::string_view record_kind,
               std::string_view key,evidence::StoredRecord& out,bool& found){
  out={};const auto r=store.latest(record_kind,key,out);
  found=r.ok();return r.code==Code::not_found?Result{}:r;
}
void guard(evidence::CommitBatch& batch,std::string_view record_kind,std::string_view key,
           const evidence::StoredRecord& previous){
  batch.guards.push_back({std::string(record_kind),std::string(key),
                          previous.revision,previous.digest});
}
bool valid(const SourceAuthority& value) {
  return valid_token(value.uid,256)&&valid_token(value.owner,256)&&
    valid_token(value.clock_domain)&&
    (value.kind==SourceAuthorityKind::local||value.kind==SourceAuthorityKind::relay);
}
Bytes encode(const SourceAuthority& value){
  return pack({"source-authority/1",value.uid,value.owner,value.clock_domain,
               value.kind==SourceAuthorityKind::local?"0":"1"});
}
}
Result SourceAuthorityEvidenceService::read(std::string_view uid,SourceAuthority& out) const {
  out={};if(!valid_token(uid,256))return fail(Code::invalid,"invalid source authority UID");
  evidence::StoredRecord record;
  if(auto r=store_.latest(kind,sha256(uid),record);!r)return r;
  std::vector<std::string> fields;
  if(record.record.tombstone||!unpack(record.record.payload,fields,5,4096)||
     fields.size()!=5||fields[0]!="source-authority/1"||
     fields[1]!=uid||!valid_token(fields[2],256)||!valid_token(fields[3])||
     (fields[4]!="0"&&fields[4]!="1"))
    return fail(Code::corrupt,"invalid immutable source authority");
  out={fields[1],fields[2],fields[3],
       fields[4]=="0"?SourceAuthorityKind::local:SourceAuthorityKind::relay};
  if(!valid(out)||encode(out)!=record.record.payload||
     record.record.key!=sha256(uid)||record.record.kind!=kind)
    return fail(Code::corrupt,"source authority record witness changed");
  return {};
}
Result SourceAuthorityEvidenceService::stage(evidence::CommitBatch& batch,
                                              const SourceAuthority& authority) const {
  if(!valid(authority))return fail(Code::invalid,"invalid source authority identity");
  const auto key=sha256(authority.uid),payload=encode(authority);
  const auto staged=std::find_if(batch.records.begin(),batch.records.end(),[&](const auto& record){
    return record.kind==kind&&record.key==key;
  });
  if(staged!=batch.records.end())
    return !staged->tombstone&&staged->payload==payload?Result{}:
      fail(Code::unauthorized,"one batch cannot change source authority");
  const auto foreign_kind=authority.kind==SourceAuthorityKind::local?
    "mesh.receive.owner":"lifecycle.fence";
  evidence::StoredRecord foreign;bool foreign_found=false;
  if(auto r=inspect(store_,foreign_kind,key,foreign,foreign_found);!r)return r;
  if(foreign_found)return fail(Code::unauthorized,"source UID belongs to the other ownership role");
  guard(batch,foreign_kind,key,foreign);
  evidence::StoredRecord previous;bool found=false;
  if(auto r=inspect(store_,kind,key,previous,found);!r)return r;
  if(found) {
    SourceAuthority checked;
    if(auto r=read(authority.uid,checked);!r)return r;
    if(checked.owner!=authority.owner||checked.clock_domain!=authority.clock_domain||
       checked.kind!=authority.kind)
      return fail(Code::unauthorized,"source UID owner, clock and role are immutable");
  }
  guard(batch,kind,key,previous);
  if(!found)batch.records.push_back({std::string(kind),key,payload,{},false});
  return {};
}
}

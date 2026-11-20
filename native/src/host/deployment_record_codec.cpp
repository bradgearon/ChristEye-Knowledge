#include "deployment_record_codec.hpp"
#include <charconv>
#include <utility>
namespace anaiero::deployment {
namespace recordcodec {
bool number(std::string_view text,std::uint64_t& value){
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
  return !text.empty()&&parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&std::to_string(value)==text;
}
bool valid(const Package& p){
  return !p.directory.empty()&&p.directory.size()<=32768&&p.directory.find('\0')==p.directory.npos&&
    valid_digest(p.manifest_sha256)&&valid_digest(p.host_sha256)&&valid_digest(p.module_sha256);
}
Bytes package_wire(const Package& p){return pack({p.directory,p.manifest_sha256,p.host_sha256,p.module_sha256});}
Result package_read(std::string_view bytes,Package& p,bool empty=false){
  std::vector<std::string> f;
  if(!unpack(bytes,f,4,40000)||f.size()!=4)return {Code::corrupt,"invalid deployment package identity"};
  p={f[0],f[1],f[2],f[3]};
  if(empty&&p==Package{})return {};
  return valid(p)?Result{}:Result{Code::corrupt,"invalid deployment package bounds"};
}
Bytes body(const Plan& p){
  std::vector<std::string> fields{p.compatibility_binding.empty()?"anaiero.deployment-plan/1":"anaiero.deployment-plan/2",p.operation_id,p.store_id,std::to_string(p.generation),p.root_sha256,
    std::to_string(p.selection_revision),p.selection_sha256,package_wire(p.candidate),package_wire(p.previous)};
  if(!p.compatibility_binding.empty())fields.push_back(p.compatibility_binding);
  return pack(fields);
}
Result validate(const Plan& p){
  if(!valid_token(p.operation_id,128)||!valid_digest(p.store_id)||
     (p.generation==0?!p.root_sha256.empty():!valid_digest(p.root_sha256))||
     !valid(p.candidate)||(p.previous!=Package{}&&!valid(p.previous))||
     (p.previous!=Package{}&&p.previous.module_sha256!=p.candidate.module_sha256&&p.compatibility_binding.empty())||
     (p.selection_revision==0?!p.selection_sha256.empty():!valid_digest(p.selection_sha256))||
     !valid_digest(p.plan_sha256)||sha256(body(p))!=p.plan_sha256)
    return {Code::corrupt,"deployment plan identity or digest is invalid"};
  if(!p.compatibility_binding.empty()){
    Binding binding;if(auto r=decode_binding(p.compatibility_binding,binding);!r)return r;
    if(binding.target_build!=p.candidate.module_sha256)return {Code::corrupt,"plan binding target differs from candidate"};
  }
  return {};
}
std::string operation(const Plan& p){return "deployment-"+p.operation_id;}

Result decode_selection(std::string_view bytes,std::string_view expected_store,Selection& out){
  std::vector<std::string> f;
  if(!unpack(bytes,f,6,128U*1024U)||f.size()!=6||
     f[0]!="anaiero.deployment-selection/1"||f[1]!=expected_store||
     !valid_token(f[4],128)||!valid_digest(f[5]))
    return {Code::corrupt,"invalid canonical deployment selection"};
  Selection result;
  if(auto r=package_read(f[2],result.current);!r)return r;
  if(auto r=package_read(f[3],result.previous,true);!r)return r;
  result.operation_id=f[4];result.plan_sha256=f[5];out=std::move(result);return {};
}
}
using recordcodec::body;using recordcodec::package_read;using recordcodec::validate;
using recordcodec::number;
Bytes encode_plan(const Plan& p){return pack({body(p),p.plan_sha256});}
Result decode_plan(std::string_view bytes,Plan& p){
  std::vector<std::string> outer,f;Plan next;
  if(!unpack(bytes,outer,2,128U*1024U)||outer.size()!=2||
     !unpack(outer[0],f,10,128U*1024U)||!((f.size()==9&&f[0]=="anaiero.deployment-plan/1")||(f.size()==10&&f[0]=="anaiero.deployment-plan/2"))||
     !number(f[3],next.generation)||!number(f[5],next.selection_revision))
    return {Code::corrupt,"invalid deployment plan artifact"};
  next.operation_id=f[1];next.store_id=f[2];next.root_sha256=f[4];next.selection_sha256=f[6];next.plan_sha256=outer[1];
  if(auto r=package_read(f[7],next.candidate);!r)return r;
  if(auto r=package_read(f[8],next.previous,true);!r)return r;
  if(f.size()==10)next.compatibility_binding=f[9];
  if(auto r=validate(next);!r)return r;
  p=std::move(next);return {};
}
}

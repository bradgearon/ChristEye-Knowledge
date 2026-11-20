#include "anaiero/sdk/engine.hpp"
#include <array>
#include <charconv>

namespace anaiero::sdk {
namespace {
bool number(std::string_view text,std::uint64_t& value) {
  if(text.empty())return false;
  const auto read=std::from_chars(text.data(),text.data()+text.size(),value);
  return read.ec==std::errc{}&&read.ptr==text.data()+text.size()&&std::to_string(value)==text;
}
bool valid(const MigrationRequest& request) {
  return valid_token(request.operation_id)&&valid_token(request.source_schema)&&valid_token(request.target_schema)&&
    request.source_schema!=request.target_schema&&valid_digest(request.implementation_sha256)&&
    valid_token(request.parameters.schema)&&request.parameters.payload.size()<=4U*1024U*1024U;
}
}
Bytes encode_migration_request(const MigrationRequest& request) {
  if(!valid(request))return {};
  const std::array<std::string,7> fields{"engine-migration-request/1",request.operation_id,
    std::to_string(request.expected_revision),request.source_schema,request.target_schema,
    request.implementation_sha256,encode_envelope(request.parameters)};
  return pack(fields);
}
Result decode_migration_request(std::string_view bytes,MigrationRequest& output) {
  std::vector<std::string> fields;
  const auto decoded=unpack(bytes,fields,7,4U*1024U*1024U+8192U);
  if(!decoded)return decoded;
  MigrationRequest request;
  if(fields.size()!=7||fields[0]!="engine-migration-request/1"||
     !number(fields[2],request.expected_revision)||!decode_envelope(fields[6],request.parameters))
    return {Code::corrupt,"migration request codec"};
  request.operation_id=fields[1];request.source_schema=fields[3];
  request.target_schema=fields[4];request.implementation_sha256=fields[5];
  if(!valid(request)||encode_migration_request(request)!=bytes)return {Code::corrupt,"migration request bounds"};
  output=std::move(request);return {};
}
Digest migration_request_digest(const EngineIdentity& identity,const MigrationRequest& request) {
  const auto encoded=encode_migration_request(request);
  if(!valid_identity(identity)||encoded.empty())return {};
  const StepRequest pseudo{request.operation_id,request.expected_revision,
    {"anaiero.engine-migration/1",encoded}};
  return request_digest(identity,pseudo);
}
Bytes encode_migration_plan(const MigrationPlan& plan) {
  const auto request=encode_migration_request(plan.request);
  const auto witness=migration_request_digest(plan.identity,plan.request);
  if(request.empty()||!valid_identity(plan.identity)||!valid_digest(plan.source_checkpoint_sha256)||
     !valid_digest(plan.target_checkpoint_sha256)||!valid_digest(witness))return {};
  const std::array<std::string,6> fields{"engine-migration-plan/1",encode_identity(plan.identity),request,
    plan.source_checkpoint_sha256,plan.target_checkpoint_sha256,witness};
  return pack(fields);
}
Result decode_migration_plan(std::string_view bytes,MigrationPlan& output) {
  std::vector<std::string> fields;
  const auto decoded=unpack(bytes,fields,6,4U*1024U*1024U+16384U);
  if(!decoded)return decoded;
  MigrationPlan plan;
  if(fields.size()!=6||fields[0]!="engine-migration-plan/1"||
     !decode_identity(fields[1],plan.identity)||!decode_migration_request(fields[2],plan.request))
    return {Code::corrupt,"migration plan codec"};
  plan.source_checkpoint_sha256=fields[3];plan.target_checkpoint_sha256=fields[4];
  if(!valid_digest(fields[5])||fields[5]!=migration_request_digest(plan.identity,plan.request)||
     encode_migration_plan(plan)!=bytes)return {Code::corrupt,"migration plan witness"};
  output=std::move(plan);return {};
}
} // namespace anaiero::sdk

#include "anaiero/sdk/migration_receipt_codec.hpp"
#include <utility>

namespace anaiero::sdk {
Bytes encode_migration_receipt(const MigrationReceipt& receipt) {
  const auto plan=encode_migration_plan(receipt.plan);
  if(plan.empty()||!valid_digest(receipt.transition_sha256))return {};
  return pack({"engine-migration-receipt/1",plan,receipt.transition_sha256});
}
Result decode_migration_receipt(std::string_view bytes,MigrationReceipt& result) {
  std::vector<std::string> fields;
  const auto parsed=unpack(bytes,fields,3,4U*1024U*1024U+32768U);
  if(!parsed)return parsed;
  MigrationReceipt decoded;
  if(fields.size()!=3||fields[0]!="engine-migration-receipt/1"||
     !decode_migration_plan(fields[1],decoded.plan)||!valid_digest(fields[2]))
    return {Code::corrupt,"invalid retained migration receipt"};
  decoded.transition_sha256=fields[2];
  if(encode_migration_receipt(decoded)!=bytes)
    return {Code::corrupt,"noncanonical retained migration receipt"};
  result=std::move(decoded);return {};
}
} // namespace anaiero::sdk

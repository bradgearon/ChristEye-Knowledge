#pragma once
// Binary and identity formats for native mesh / retention records. No transport,
// authorization decisions or canonical commit side effects are owned here.
#include "anaiero/sdk/mesh.hpp"

namespace anaiero::sdk::mesh::recordcodec {
lifecycle::SourceRef scope(const EngineIdentity&);
Bytes encode_scope(const lifecycle::SourceRef&);
Bytes encode_cursor(const lifecycle::Cursor&);
bool decode_cursor(std::string_view,lifecycle::Cursor&);
std::string grant_key(std::string_view,const lifecycle::SourceRef&);
std::string receive_key(std::string_view,std::string_view,const lifecycle::SourceRef&);
std::string received_observation_key(std::string_view,const lifecycle::SourceRef&,std::uint64_t);
std::string operation(std::string_view action,std::string_view bytes);
Bytes offer_metadata(const BootstrapOffer&);
Result decode_receive(const evidence::StoredRecord&,ReceiveReceipt&);
Bytes encode_retention_request(const ReceiverRetentionRequest&);
bool valid_retention_request(const ReceiverRetentionRequest&);
Bytes encode_retention_plan(const ReceiverRetentionPlan&);
Result decode_retention_plan(std::string_view,ReceiverRetentionPlan&);
}

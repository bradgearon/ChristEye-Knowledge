#pragma once
// Typed proof reader for experiment outcomes. Session/metric binary framing
// remains in the shared codecs; this adapter verifies canonical blob witnesses.
#include "anaiero/sdk/experiment_history.hpp"
#include "anaiero/sdk/session_record_codec.hpp"
namespace anaiero::sdk::experiment_evidence {
Result object(evidence::ICommitStore&,std::string_view kind,std::string_view key,
              evidence::StoredRecord&,Bytes&);
Result publish(evidence::ICommitStore&,std::string_view kind,std::string_view key,std::string operation,
               Bytes,std::vector<Digest> references,std::vector<evidence::RecordGuard> guards={});
std::string operation_id(std::string_view prefix,std::string_view identifier);
Result canonical_engine(evidence::ICommitStore&,std::string_view instance,
    evidence::StoredRecord&,EngineIdentity&,EngineState&,PreparedTransition&,std::vector<Digest>&);
Result observations(evidence::ICommitStore&,const EngineIdentity&,const EngineState&,
    std::vector<Observation>&,std::vector<evidence::RecordGuard>&);
}

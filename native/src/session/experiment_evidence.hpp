#pragma once
// Typed proof reader for experiment outcomes. Session/metric binary framing
// remains in the shared codecs; this adapter verifies canonical blob witnesses.
#include "anaiero/sdk/experiment_history.hpp"
#include "anaiero/sdk/session_record_codec.hpp"
namespace anaiero::sdk::experiment_evidence {
Result canonical_engine(evidence::ICommitStore&,std::string_view instance,
    evidence::StoredRecord&,EngineIdentity&,EngineState&,PreparedTransition&,std::vector<Digest>&);
Result observations(evidence::ICommitStore&,const EngineIdentity&,const EngineState&,
    std::vector<Observation>&,std::vector<evidence::RecordGuard>&);
}

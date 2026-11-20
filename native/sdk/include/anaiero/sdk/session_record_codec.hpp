#pragma once
// Versioned wire formats for canonical session records. Domain services see
// typed values rather than decoding the underlying binary framing.
#include "anaiero/sdk/engine.hpp"

namespace anaiero::sdk {
struct SessionEngineRecord {
  EngineIdentity identity;
  Digest checkpoint_sha256,transition_sha256;
};
struct SessionMetricRecord {
  EngineIdentity identity;
  Observation observation;
  std::uint64_t revision=0;
};
Bytes encode_session_engine_record(const SessionEngineRecord&);
Result decode_session_engine_record(std::string_view,SessionEngineRecord&);
Bytes encode_session_metric_record(const SessionMetricRecord&);
Result decode_session_metric_record(std::string_view,SessionMetricRecord&);
}

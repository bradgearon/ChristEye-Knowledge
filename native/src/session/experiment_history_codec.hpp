#pragma once
// Internal schema codecs for retained experiment plans, scores and outcomes.
// Persistence/authorization are separate from this byte-oriented module.
#include "anaiero/sdk/experiment_history.hpp"

namespace anaiero::sdk::history_codec {
Result bad(std::string);
Bytes strategy_bytes(const ScoreStrategyIdentity&);
Result strategy_read(std::string_view,ScoreStrategyIdentity&);
Bytes spec_bytes(const ExperimentSpecification&);
Result spec_read(std::string_view,ExperimentSpecification&);
Bytes score_bytes(const Score&);
Result score_read(std::string_view,Score&);
Bytes trial_bytes(const TrialResult&);
Result trial_read(std::string_view,TrialResult&);
Bytes result_bytes(const ExperimentResult&);
Result result_read(std::string_view,ExperimentResult&);
Bytes review_bytes(const OutcomeReview&,std::string_view);
Result review_read(std::string_view,std::string_view,OutcomeReview&);
Bytes encode_intent(const ExperimentSpecification&,const EngineIdentity&,const EngineState&,const ScoreStrategyIdentity&);
Result read_intent(std::string_view,std::string_view expected_id,RecordedExperiment&);
}

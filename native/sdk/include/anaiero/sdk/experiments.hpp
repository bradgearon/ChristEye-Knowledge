#pragma once
#include "anaiero/sdk/engine.hpp"
#include "anaiero/sdk/observer.hpp"
namespace anaiero::sdk {
struct TrialProgram { std::string id;std::vector<Envelope> inputs;Envelope change;bool has_change=false; };
struct TrialResult { std::string id;EngineState final_state;std::vector<Observation> observations;Digest receipt_tip; };
struct TrialCandidate {std::string id;Envelope change;};
struct ExperimentRequest {
  std::string id;
  std::uint64_t expected_revision=0;
  Digest expected_state_sha256;
  std::vector<Envelope> inputs;
  std::vector<TrialCandidate> candidates;
};
struct MetricComparison {Observation baseline,candidate;};
struct Score {std::int64_t value=0;bool eligible=true;std::string rationale;};
struct ScoreStrategyIdentity {std::string id;Digest implementation_sha256;Envelope configuration;};
struct IScoreStrategy {
  virtual ~IScoreStrategy()=default;
  virtual ScoreStrategyIdentity identity()const=0;
  // A higher score is preferred. Every pair has the same metric, schema,
  // quantity, unit and logical tick; strategies retain domain-specific meaning.
  virtual Result score(const std::vector<MetricComparison>&,Score&)const=0;
};
struct CandidateResult {std::string id;Result status;TrialResult trial;Score score;};
struct ExperimentResult {
  EngineIdentity source;
  std::uint64_t source_revision=0;
  Digest source_state_sha256,program_sha256;
  ScoreStrategyIdentity strategy;
  TrialResult baseline;
  Score baseline_score;
  std::vector<CandidateResult> candidates;
  std::string chosen_candidate; // Empty means preserve the baseline.
};
struct OutcomeEvidence {
  EngineIdentity identity;
  EngineState state;
  std::vector<Observation> observations;
  Digest receipt_tip;
};
struct OutcomeReview {
  Score measured_score;
  bool meets_selected_score=false,exact_selected_state=false;
  Digest expected_state_sha256,observed_state_sha256,observed_receipt_tip;
};
class ExperimentService {
  IObserverSink* observer_=nullptr;
public:
  void set_observer_sink(IObserverSink* observer)noexcept{observer_=observer;}
  // Same module interpreter, independent engine; never mutates the live source.
  Result evaluate(IEngine& source,const TrialProgram&,TrialResult&) const;
  Result replay_equivalence(IEngine& source,const TrialProgram&,const EngineState& expected,TrialResult&) const;
  Result select(IEngine& source,const ExperimentRequest&,const IScoreStrategy&,ExperimentResult&) const;
  Result compare_outcome(const ExperimentResult&,const OutcomeEvidence&,const IScoreStrategy&,OutcomeReview&) const;
};
}

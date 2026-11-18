#pragma once
#include "anaiero/sdk/experiments.hpp"
#include "anaiero/sdk/runtime.hpp"
#include "anaiero/sdk/evidence.hpp"
namespace anaiero::sdk {
struct ExperimentSpecification {
  std::string id;
  std::vector<Envelope> inputs;
  std::vector<TrialCandidate> candidates;
};
struct RecordedExperiment {
  ExperimentSpecification specification;
  EngineIdentity source;
  EngineState source_state;
  ScoreStrategyIdentity strategy;
  bool completed=false;
  ExperimentResult result;
};
// Serial owner service. The source, input program and strategy are committed
// before isolated work starts. An interrupted run resumes from that frozen
// source; pure bounded trials may be recomputed, with one durable final result.
class ExperimentHistoryService {
  EngineModuleCatalog& catalog_;
  evidence::ICommitStore& store_;
public:
  ExperimentHistoryService(EngineModuleCatalog& catalog,evidence::ICommitStore& store):catalog_(catalog),store_(store){}
  Result select(IEngine&,const ExperimentSpecification&,const IScoreStrategy&,RecordedExperiment&);
  Result load(std::string_view id,RecordedExperiment&);
  // Application is explicit and requires the original live source revision.
  Result apply_selected(std::string_view id,std::string operation_id,SessionService&);
  // One frozen measured review per experiment, from canonical engine evidence.
  Result review(std::string_view id,std::string operation_id,const IScoreStrategy&,OutcomeReview&);
  // Drops live object roots. Audit identities remain; experiment IDs are never reused.
  Result release(std::string_view id,std::string operation_id);
};
}

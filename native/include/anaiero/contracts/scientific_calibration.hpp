#pragma once
// Passive calibration records and typed, compiled extension ports. These
// records preserve declared provenance; they do not authenticate an instrument.
#include "anaiero/models/environment_compartment.hpp"
#include "anaiero/models/scientific_quantity.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace anaiero::science::calibration {
inline constexpr std::size_t kMaxCandidates=32,kMaxSamples=4096;
enum class ReferenceOrigin : std::uint8_t {synthetic_reference=1,external_unverified_readonly=2};
enum class Code : std::uint8_t {ok,invalid_plan,invalid_reference,contaminated_split,
  model_failure,cancelled,reentrant,invalid_policy};
enum class Phase : std::uint8_t {validated=1,training_complete,winner_frozen,holdout_complete,rejected};
struct Provenance {
  ReferenceOrigin origin=ReferenceOrigin::synthetic_reference;
  std::uint32_t source=0,clock_domain=0;
  std::uint64_t epoch=0;
  std::string run_id,producer_sha256,protocol_sha256,raw_sha256;
};
struct Sample {
  std::uint64_t sequence=0,acquired_ns=0;
  std::int32_t inlet_input=0;
  std::int64_t concentration_micromolar=0,uncertainty_micromolar=0;
};
struct Reference {
  Provenance provenance;
  Unit unit=Unit::micromolar;
  std::uint64_t step_ns=0;
  std::int64_t initial_micromolar=0,initial_uncertainty_micromolar=0;
  std::string sha256;
  std::vector<Sample> samples;
};
struct Candidate {
  std::string id,model_sha256;
  EnvironmentCompartmentModel model{};
};
struct FidelityCriteria {
  // Raw absolute errors are tested. Declared measurement uncertainty is
  // retained separately and never silently used to relax either threshold.
  std::uint64_t max_absolute_error_micromolar=0,max_rmse_micromolar=0;
};
struct Plan {
  std::string name,sha256,training_sha256,holdout_sha256;
  FidelityCriteria criteria;
  std::vector<Candidate> candidates;
};
struct ErrorSummary {
  std::uint64_t count=0,sum_squared_error=0,max_absolute_error_micromolar=0,
    max_reference_uncertainty_micromolar=0;
};
struct CandidateResult {
  std::string id,model_sha256;
  ErrorSummary training;
};
struct Report {
  Code code=Code::invalid_plan;
  bool passed=false,training_passed=false,holdout_passed=false,winner_frozen=false;
  std::size_t selected_index=0;
  std::string plan_name,plan_sha256,training_sha256,holdout_sha256,
    behavior_sha256,selection_policy,fidelity_policy,frozen_candidate_id,frozen_model_sha256;
  std::int64_t initial_micromolar=0,training_initial_uncertainty_micromolar=0,holdout_initial_uncertainty_micromolar=0;
  std::uint64_t step_ns=0;
  Provenance training_provenance,holdout_provenance;
  FidelityCriteria criteria;
  std::vector<CandidateResult> candidates;
  ErrorSummary holdout;
};
struct Event {
  Phase phase=Phase::validated;
  std::size_t candidate_index=0;
  std::uint64_t samples=0;
  Code code=Code::ok;
};
struct IObserver {
  virtual ~IObserver()=default;
  // Trusted compiled observers must not block or reenter the owning service.
  virtual void on_calibration_event(const Event&)noexcept=0;
};
struct ICancellation {
  virtual ~ICancellation()=default;
  virtual bool cancelled()const noexcept=0;
};
struct IEnvironmentTrial {
  virtual ~IEnvironmentTrial()=default;
  virtual std::string_view behavior_sha256()const noexcept=0;
  virtual bool begin(const EnvironmentCompartmentModel&,std::uint64_t step_ns)noexcept=0;
  virtual bool advance(std::uint64_t sequence,std::int32_t inlet_input,
                       std::int64_t& concentration_micromolar)noexcept=0;
};
struct IFidelityPolicy {
  virtual ~IFidelityPolicy()=default;
  virtual std::string_view id()const noexcept=0;
  virtual bool accepts(const ErrorSummary&,const FidelityCriteria&)const noexcept=0;
};
} // namespace anaiero::science::calibration

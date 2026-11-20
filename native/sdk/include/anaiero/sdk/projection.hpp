#pragma once
#include "anaiero/sdk/engine.hpp"
#include <memory>
namespace anaiero::sdk {
struct MetricView { EngineIdentity identity;Observation observation;std::uint64_t revision=0,generation=0; };
class ProjectionService {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  explicit ProjectionService(std::size_t capacity=512);
  ~ProjectionService();
  Result can_apply(const PreparedTransition&) const;
  Result apply(const PreparedTransition&,std::uint64_t committed_generation);
  Result replace(const std::vector<MetricView>& committed);
  void retire(std::string_view source_uid,std::uint64_t epoch);
  void clear();
  std::vector<MetricView> snapshot() const;
  std::size_t size() const noexcept;
};
}

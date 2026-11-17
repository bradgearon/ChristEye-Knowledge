#pragma once
// Optional *offline*, normalized environment feature replay: no live device I/O.
#include "anaiero/contracts/scientific_trace.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>
namespace anaiero::science {
class RecordedEnvironmentService final:public IEnvironmentFeatureModel {
  ScientificTrace trace_;
  std::uint16_t cursor_=0;
public:
  explicit RecordedEnvironmentService(ScientificTrace trace) noexcept:trace_(std::move(trace)){}
  ModelDescriptor descriptor()const noexcept override {
    return {trace_.provider,trace_.evidence,1,trace_.source_epoch,63,
      "pinned-offline-environment-trace/1","DIMENSIONLESS_ENVIRONMENT_PROXY"};
  }
  bool advance(State& s,std::uint64_t step) noexcept override {
    if(trace_.channel!=Channel::environment_proxy||
       (trace_.provider!=Provider::native_reference&&
        trace_.provider!=Provider::physicell&&trace_.provider!=Provider::moose&&
        trace_.provider!=Provider::steps)||
       s.epoch!=trace_.source_epoch||step!=static_cast<std::uint64_t>(cursor_)+1||
       cursor_>=trace_.count)return false;
    s.environment=trace_.features[cursor_++];return true;
  }
};
} // namespace anaiero::science

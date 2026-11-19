#pragma once
#include "mixed_qualification.hpp"
#include <chrono>
#include <filesystem>
#include <map>
namespace anaiero::qualification::mixed_detail {
using Clock=std::chrono::steady_clock;
std::uint64_t elapsed(Clock::time_point);
std::string utc();
std::string utf8(const std::filesystem::path&);
void violation(MixedReport&,std::string);
sdk::host::Status snapshot(sdk::host::NativeContinuousHost&,MixedReport&);
sdk::Result command(sdk::host::NativeContinuousHost&,sdk::host::Command,MixedReport&,Clock::time_point,bool);
sdk::Result through(sdk::host::NativeContinuousHost&,std::uint64_t,MixedReport&,Clock::time_point);
sdk::Result pressure(sdk::host::NativeContinuousHost&,MixedReport&,Clock::time_point);
sdk::Result probe(sdk::host::NativeContinuousHost&,const sdk::EngineIdentity&,MixedReport&,Clock::time_point);
sdk::Result inventory(std::string_view,const sdk::host::Status&,MixedReport&);
sdk::Result quota_probe(std::string_view,MixedReport&);
std::map<std::string,sdk::Digest> state_digests(const sdk::SessionView&);
void configure_limits(MixedReport&);
sdk::Result finalize(sdk::Result,Clock::time_point,MixedReport&);
sdk::Result final_engines(const sdk::SessionDefinition&,const sdk::SessionView&,MixedReport&);
class TimingCollector final:public sdk::host::ITimingObserver {
  MixedReport& report_;
public:
  explicit TimingCollector(MixedReport& report):report_(report){}
  void observe(const sdk::host::TimingObservation&) noexcept override;
};
}

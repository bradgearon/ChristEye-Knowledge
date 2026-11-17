#pragma once
// SW7.1 services adapt live, checked model features to the current SW7
// multirate C++ coordinator. Biological model evidence remains unverified.
#include "anaiero/contracts/live_solver.hpp"
#include "anaiero/services/reference_electrical_service.hpp"
#include <cstdint>
#include <string>
#include <utility>
namespace anaiero::science {
class LiveElectricalFeatureService final:public IElectricalFeatureModel {
  ILiveSolverExchange& io_;
  std::string hash_;
  std::uint32_t epoch_,source_;
  bool poisoned_=false;
public:
  LiveElectricalFeatureService(ILiveSolverExchange& io,std::string model_hash,
    std::uint32_t epoch,std::uint32_t source=64):
    io_(io),hash_(std::move(model_hash)),epoch_(epoch),source_(source){}
  ModelDescriptor descriptor()const noexcept override {
    return {Provider::neuron,Evidence::published_model_unverified,1,epoch_,
      source_,"live-neuron-model/1","NORMALIZED_ACTIVITY_PROXY"};
  }
  bool advance(State& state,const Input& input,std::uint32_t) noexcept override {
    if(poisoned_||state.epoch!=epoch_||state.fast_steps>=4096||
       input.pattern_proxy<0||input.pattern_proxy>kProxyMax)return false;
    LiveResponse reply{};
    const LiveRequest request{Channel::electrical_activity,epoch_,
      state.fast_steps+1,kLiveFastStepNs,input.pattern_proxy,hash_};
    if(!io_.advance(request,reply)){poisoned_=true;return false;}
    state.activity=reply.normalized_feature;
    const auto active=input.phase==Phase::train?state.activity/7:0;
    state.eligibility=proxy_bound(state.eligibility-state.eligibility/24+active);
    ++state.fast_steps;return true;
  }
  bool poisoned()const noexcept{return poisoned_;}
};
class LiveEnvironmentFeatureService final:public IEnvironmentFeatureModel {
  ILiveSolverExchange& io_;
  std::string hash_;
  std::uint32_t epoch_,source_;
  bool poisoned_=false;
public:
  LiveEnvironmentFeatureService(ILiveSolverExchange& io,std::string model_hash,
    std::uint32_t epoch,std::uint32_t source=65):
    io_(io),hash_(std::move(model_hash)),epoch_(epoch),source_(source){}
  ModelDescriptor descriptor()const noexcept override {
    return {Provider::physicell,Evidence::published_model_unverified,1,epoch_,
      source_,"live-environment-feature/1","NORMALIZED_ENVIRONMENT_PROXY"};
  }
  bool advance(State& state,std::uint64_t macro_tick) noexcept override {
    if(poisoned_||state.epoch!=epoch_||macro_tick==0||macro_tick>4096)return false;
    LiveResponse reply{};
    const LiveRequest request{Channel::environment_proxy,epoch_,macro_tick,
      kLiveSlowStepNs,0,hash_};
    if(!io_.advance(request,reply)){poisoned_=true;return false;}
    state.environment=reply.normalized_feature;
    return true;
  }
  bool poisoned()const noexcept{return poisoned_;}
};
} // namespace anaiero::science

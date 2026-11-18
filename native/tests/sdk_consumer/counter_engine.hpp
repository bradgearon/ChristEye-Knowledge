#pragma once
// Independent SDK consumer: no repository implementation headers are available.
#include <anaiero/sdk/engine.hpp>
#include <charconv>
#include <atomic>
#include <chrono>
#include <limits>
#include <thread>
#if __has_include("anaiero/application/session_runner.hpp") || __has_include("christeye/living/learning_plan.hpp")
#error "Legacy implementation headers leaked through the public SDK target"
#endif

namespace independent {
using namespace anaiero::sdk;
inline std::atomic<std::uint64_t> fork_calls=0;
inline EngineDescriptor descriptor() {
  EngineDescriptor value;value.type_id="independent.counter";value.display_name="Independent counter";
  value.module_sha256=sha256("independent-counter-linked-build/1");value.behavior_id="bounded-counter/1";
  value.configuration_schemas={"counter.configuration/1"};value.input_schemas={"counter.add/1"};
  value.checkpoint_schemas={"counter.checkpoint/1"};value.observation_schemas={"counter.observation/1"};
  value.change_schemas={"counter.set/1"};value.finite=true;value.isolated_trials=true;value.change_preparation=true;
  return value;
}
inline Envelope configuration(std::uint64_t ticks=8,std::size_t padding=0) {
  return {"counter.configuration/1",pack({std::to_string(ticks),std::string(padding,'x')})};
}
inline bool integer(std::string_view text,std::uint64_t& out) {
  auto result=std::from_chars(text.data(),text.data()+text.size(),out);
  return result.ec==std::errc{}&&result.ptr==text.data()+text.size()&&std::to_string(out)==text;
}
inline std::uint64_t count(const EngineState& state) {
  std::vector<std::string> fields;std::uint64_t value=0;
  if(!unpack(state.checkpoint.payload,fields,2)||fields.size()!=2||!integer(fields[0],value))return UINT64_MAX;
  return value;
}
class Counter final:public IEngine,public ITrialCapability,public IChangeCapability {
  EngineIdentity identity_;EngineState state_;std::uint64_t maximum_ticks_;std::string padding_;
  Result prepare(const StepRequest& request,PreparedTransition& transition,bool change) {
    if(request.expected_revision!=state_.revision)return {Code::conflict,"counter revision"};
    if(state_.phase==Phase::completed)return {Code::completed,"counter complete"};
    if(request.input.payload=="delay")std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if(request.input.payload=="mutate") {++state_.revision;return {Code::io_error,"deliberate conformance fault"};}
    std::uint64_t amount=0;
    if(!integer(request.input.payload,amount))return {Code::invalid,"counter integer input"};
    const auto previous=count(state_);
    if(previous==UINT64_MAX||amount>static_cast<std::uint64_t>(INT64_MAX)||
        (!change&&previous>static_cast<std::uint64_t>(INT64_MAX)-amount))return {Code::capacity,"counter value bound"};
    transition.identity=identity_;transition.operation_id=request.operation_id;
    transition.request_sha256=request_digest(identity_,request);transition.expected_state_sha256=state_digest(state_);
    transition.expected_revision=state_.revision;transition.target=state_;++transition.target.revision;
    if(!change)++transition.target.tick;
    const auto value=change?amount:previous+amount;
    transition.target.checkpoint={"counter.checkpoint/1",pack({std::to_string(value),padding_})};
    transition.target.phase=change&&state_.phase==Phase::paused?Phase::paused:
      transition.target.tick>=maximum_ticks_?Phase::completed:Phase::running;
    transition.observations={{"counter.observation/1","count","dimensionless_count","count",static_cast<std::int64_t>(value),transition.target.tick}};
    transition.receipts={{"counter.receipt/1",pack({std::to_string(previous),std::to_string(value)})}};
    return {};
  }
public:
  Counter(EngineIdentity identity,Envelope config,std::uint64_t maximum,std::string padding):
    identity_(std::move(identity)),maximum_ticks_(maximum),padding_(std::move(padding)) {
    state_.configuration=std::move(config);state_.checkpoint={"counter.checkpoint/1",pack({"0",padding_})};
  }
  const EngineIdentity& identity()const noexcept override{return identity_;}
  const EngineState& state()const noexcept override{return state_;}
  Result prepare_step(const StepRequest& request,PreparedTransition& transition)override{return prepare(request,transition,false);}
  Result prepare_change(const StepRequest& request,PreparedTransition& transition)override{return prepare(request,transition,true);}
  Result install(const EngineState& state)override {
    if(state.revision!=state_.revision+1||state.tick<state_.tick)return {Code::conflict,"counter install revision"};
    return restore(state);
  }
  Result restore(const EngineState& state)override {
    if(!valid_state(state)||state.configuration!=state_.configuration||count(state)==UINT64_MAX||state.tick>maximum_ticks_)
      return {Code::invalid,"counter checkpoint"};
    state_=state;return {};
  }
  ITrialCapability* trials()noexcept override{return this;}
  IChangeCapability* changes()noexcept override{return this;}
  Result fork(std::unique_ptr<IEngine>& output)const override {++fork_calls;output=std::make_unique<Counter>(*this);return {};}
};
class Factory final:public IEngineFactory {
public:
  Result create(const EngineIdentity& identity,const Envelope& config,const EngineState* restored,
      std::unique_ptr<IEngine>& output)override {
    std::vector<std::string> fields;std::uint64_t maximum=0;
    if(config.schema!="counter.configuration/1"||!unpack(config.payload,fields,2)||fields.size()!=2||
        !integer(fields[0],maximum)||!maximum||maximum>1000000)return {Code::invalid,"counter configuration"};
    auto engine=std::make_unique<Counter>(identity,config,maximum,std::move(fields[1]));
    if(restored){auto result=engine->restore(*restored);if(!result)return result;}
    output=std::move(engine);return {};
  }
};
inline EngineIdentity identity(const EngineDescriptor& descriptor,const Envelope& config,std::string instance="counter") {
  return {instance,descriptor.type_id,descriptor.module_sha256,sha256(encode_envelope(config)),descriptor.behavior_id,
    "source-"+instance,"logical",1,0};
}
}

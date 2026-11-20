#include "common.hpp"
#include "christeye/living/learning_plan.hpp"
#include <algorithm>
#include <memory>
#include <sstream>
namespace anaiero::engines {
namespace detail {
namespace {
namespace pl=christeye::living::plans;
namespace process=christeye::living::process;
constexpr std::string_view config_schema="anaiero.plan-bundle/1";
constexpr std::string_view input_schema="anaiero.plan-input/1";
constexpr std::string_view checkpoint_schema="anaiero.plan-checkpoint/1";
constexpr std::string_view observation_schema="anaiero.plan-observation/1";
struct PlanData {
  pl::Plan plan;process::ProcessMachine base;std::vector<pl::ArtifactBytes> assets;
  std::uint64_t total_ticks=0;
};
Result unpack_plan(const Envelope& configuration,PlanData& data){
  Bundle bundle;
  if(configuration.schema!=config_schema||!decode(configuration.payload,bundle)||
     !bundle.contains("plan")||!bundle.contains("base"))return invalid("invalid retained VLM plan bundle");
  // ProcessMachine owns bounded inline model storage. Keep staging on the heap
  // so nested admission/recovery also fits native hosts with a 1 MiB stack.
  auto retained=std::make_unique<PlanData>();auto& candidate=*retained;
  if(pl::Plan::parse(bundle.at("plan"),candidate.plan).error!=pl::Error::ok||
     process::ProcessMachine::deserialize(bundle.at("base"),candidate.base,false).error!=process::Error::ok)
    return invalid("invalid CEVPLAN/1 or CEVLM/2 input");
  if(candidate.plan.stages().size()>8)return {Code::capacity,"VLM plans admit at most eight stages"};
  for(const auto& stage:candidate.plan.stages())candidate.total_ticks+=stage.duration;
  if(candidate.total_ticks>128||candidate.base.tick()>std::numeric_limits<std::uint64_t>::max()-candidate.total_ticks)
    return {Code::capacity,"VLM plans admit at most 128 logical ticks"};
  for(const auto& task:candidate.base.tasks())
    if(!task.completed)return invalid("VLM plan base must have no active tasks");
  if(candidate.base.tasks().size()+candidate.plan.stages().size()>process::kMaxTasks||
     candidate.base.journal().size()+candidate.plan.stages().size()+candidate.total_ticks>process::kMaxEntries||
     candidate.base.events().size()+candidate.total_ticks>process::kMaxEvents)
    return {Code::capacity,"VLM base leaves insufficient task, event or journal capacity"};
  if(bundle.size()!=candidate.plan.artifacts().size()+2)return invalid("unexpected retained VLM assets");
  for(const auto& asset:candidate.plan.artifacts()){
    const auto it=bundle.find("asset/"+asset.relative_path);
    if(it==bundle.end())return invalid("missing retained VLM asset");
    candidate.assets.push_back({asset.relative_path,it->second});
  }
  auto check=std::make_unique<pl::Session>();
  if(check->initialize(candidate.plan,candidate.base,candidate.assets)!=pl::Error::ok)
    return invalid("VLM plan mappings, base identity or retained artifact verification failed");
  data=std::move(candidate);return {};
}
std::string receipts(const pl::Session& session){
  std::ostringstream result;
  for(const auto& event:session.progress())pl::write_progress_json(result,event);
  return result.str();
}
std::string plan_checkpoint(const pl::Session& session){
  const auto steps=session.progress().size();
  const auto stages=std::count_if(session.progress().begin(),session.progress().end(),
    [](const auto& event){return event.completed;});
  return encode({{"steps",std::to_string(steps)},{"stages",std::to_string(stages)},
    {"image",session.machine().serialize(true)},{"receipts",receipts(session)},
    {"receipt_tip",steps?session.progress().back().receipt_hash:std::string(64,'0')}});
}
Result replay(const PlanData& data,const EngineState& state,pl::Session& session){
  if(!valid_base(state,config_schema,checkpoint_schema))return corrupt("VLM checkpoint schema mismatch");
  Bundle checkpoint;std::uint64_t steps=0;
  if(!decode(state.checkpoint.payload,checkpoint)||
     !has_exact(checkpoint,{"steps","stages","image","receipts","receipt_tip"})||
     !unsigned_number(checkpoint.at("steps"),steps)||steps>data.total_ticks||
     state.tick!=data.base.tick()+steps)return corrupt("invalid VLM checkpoint position");
  if(session.initialize(data.plan,data.base,data.assets)!=pl::Error::ok)
    return corrupt("retained VLM initialization failed");
  // Reconstruct only the bounded committed prefix. No observer or publisher is attached.
  for(std::uint64_t tick=0;tick<steps;++tick)
    if(session.step()!=pl::Error::ok)return corrupt("VLM committed prefix no longer replays");
  if(plan_checkpoint(session)!=state.checkpoint.payload)
    return corrupt("VLM machine, stage position or original receipt tip differs on replay");
  if((session.finished()&&state.phase!=Phase::completed)||
     (!session.finished()&&state.phase==Phase::completed)||
     (steps&&state.phase==Phase::ready))return corrupt("VLM persisted phase differs from its committed program position");
  return {};
}
class PlanEngine final:public IEngine,public ITrialCapability {
  EngineIdentity identity_;EngineState state_;PlanData data_;
public:
  explicit PlanEngine(EngineIdentity identity):identity_(std::move(identity)){}
  Result initialize(const Envelope& config,const EngineState* restored){
    auto data=std::make_unique<PlanData>();
    if(auto r=unpack_plan(config,*data);!r)return r;
    if(identity_.model_sha256!=sha256(config.payload))return invalid("VLM bundle identity mismatch");
    auto initial=std::make_unique<pl::Session>();
    if(initial->initialize(data->plan,data->base,data->assets)!=pl::Error::ok)return invalid("VLM plan initialization failed");
    EngineState base;base.tick=data->base.tick();base.configuration=config;
    base.checkpoint={std::string(checkpoint_schema),plan_checkpoint(*initial)};
    return restore(restored?*restored:base);
  }
  const EngineIdentity& identity()const noexcept override{return identity_;}
  const EngineState& state()const noexcept override{return state_;}
  ITrialCapability* trials()noexcept override{return this;}
  Result restore(const EngineState& state)override{
    if(identity_.model_sha256!=sha256(state.configuration.payload))return corrupt("VLM immutable program changed");
    auto data=std::make_unique<PlanData>();if(auto r=unpack_plan(state.configuration,*data);!r)return r;
    auto verify=std::make_unique<pl::Session>();if(auto r=replay(*data,state,*verify);!r)return r;
    data_=std::move(*data);state_=state;return {};
  }
  Result install(const EngineState& state)override{return restore(state);}
  Result fork(std::unique_ptr<IEngine>& output)const override{
    auto clone=std::make_unique<PlanEngine>(identity_);
    if(auto r=clone->initialize(state_.configuration,&state_);!r)return r;
    output=std::move(clone);return {};
  }
  Result prepare_step(const StepRequest& request,PreparedTransition& output)override{
    if(auto r=guard(*this,request);!r)return r;
    if(request.input.schema!=input_schema||!request.input.payload.empty())
      return invalid("VLM input is its immutable program; step payload must be empty");
    auto staged=std::make_unique<pl::Session>();if(auto r=replay(data_,state_,*staged);!r)return r;
    if(staged->step()!=pl::Error::ok)return {Code::capacity,"VLM stage execution failed within admitted bounds"};
    EngineState target=state_;target.tick=staged->machine().tick();
    target.phase=staged->finished()?Phase::completed:Phase::running;
    target.checkpoint.payload=plan_checkpoint(*staged);
    auto transition=prepared(*this,request,std::move(target));
    const auto& event=staged->progress().back();
    const auto done=std::count_if(staged->progress().begin(),staged->progress().end(),
      [](const auto& value){return value.completed;});
    transition.observations={{std::string(observation_schema),"completed_stages","program_stage_count","count",static_cast<std::int64_t>(done),event.tick},
      {std::string(observation_schema),"learning","model_learning_proxy","synthetic_proxy",event.learning_proxy,event.tick},
      {std::string(observation_schema),"support","model_support_field","synthetic_proxy",event.support_field_proxy,event.tick},
      {std::string(observation_schema),"rna","model_rna_field","synthetic_proxy",event.rna_field_proxy,event.tick}};
    std::ostringstream receipt;pl::write_progress_json(receipt,event);
    transition.receipts.push_back({"CE.PLAN.PROGRESS/1",receipt.str()});
    if(staged->finished())transition.receipts.push_back({"anaiero.plan-completed/1",
      "CEPLAN-COMPLETED/1\n"+data_.plan.hash()+"\n"+event.receipt_hash+"\n"});
    output=std::move(transition);return {};
  }
};
class PlanFactory final:public IEngineFactory {
  Result create(const EngineIdentity& identity,const Envelope& config,const EngineState* state,
                std::unique_ptr<IEngine>& output)override{
    if(identity.type_id!=vlm_plan||identity.tick_ns||identity.clock_domain!="logical")
      return invalid("VLM requires its registered type and logical clock");
    auto engine=std::make_unique<PlanEngine>(identity);
    if(auto r=engine->initialize(config,state);!r)return r;
    output=std::move(engine);return {};
  }
};
}
Result add_plan(EngineModuleCatalog& catalog){
  auto d=descriptor(vlm_plan,"Virtual Machine and Plan Engine","anaiero-vlm-plan/1",
    std::string(config_schema),std::string(input_schema),std::string(checkpoint_schema),std::string(observation_schema));
  d.finite=true;d.isolated_trials=true;
  return catalog.add(std::move(d),std::make_shared<PlanFactory>());
}
}
sdk::Result plan_configuration(std::string_view plan,std::string_view base,
                               std::span<const RetainedAsset> assets,sdk::Envelope& output){
  detail::Bundle bundle{{"plan",std::string(plan)},{"base",std::string(base)}};
  for(const auto& asset:assets)
    if(!bundle.emplace("asset/"+asset.relative_path,asset.bytes).second)
      return detail::invalid("duplicate retained VLM artifact");
  sdk::Envelope candidate{"anaiero.plan-bundle/1",detail::encode(bundle)};
  auto data=std::make_unique<detail::PlanData>();if(auto r=detail::unpack_plan(candidate,*data);!r)return r;
  output=std::move(candidate);return {};
}
sdk::Result plan_output(const sdk::EngineState& state,std::string& image,std::string& receipts_ndjson){
  auto data=std::make_unique<detail::PlanData>();
  if(auto r=detail::unpack_plan(state.configuration,*data);!r)return r;
  auto session=std::make_unique<christeye::living::plans::Session>();
  if(auto r=detail::replay(*data,state,*session);!r)return r;
  image=session->machine().serialize(true);receipts_ndjson=detail::receipts(*session);return {};
}
}

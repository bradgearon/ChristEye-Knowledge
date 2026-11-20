#include "common.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include "anaiero/infrastructure/cell_checkpoint.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/services/cell_behavior_service.hpp"
#include "anaiero/services/digital_route_policy_service.hpp"
#include <sstream>
namespace anaiero::engines {
namespace detail {
namespace {
constexpr std::string_view config_schema="anaiero.cell-bundle/1";
constexpr std::string_view input_schema="anaiero.cell-input/1";
constexpr std::string_view change_schema="anaiero.cell-change/1";
constexpr std::string_view checkpoint_schema="CEVCELLSTATE/1";
constexpr std::string_view observation_schema="anaiero.cell-observation/1";
Result unpack_cell(const Envelope& configuration,Bundle& bundle,cells::Atlas& atlas,cells::World& initial){
  if(configuration.schema!=config_schema||!decode(configuration.payload,bundle)||
     !has_exact(bundle,{"atlas","reference","initial"}))return invalid("invalid retained Cell Behavior bundle");
  cells::files::SourceRecord reference;
  if(cells::files::parse_atlas(bundle.at("atlas"),atlas)!=cells::Status::ok||
     cells::files::parse_source(bundle.at("reference"),reference)!=cells::Status::ok||
     cells::files::validate_evidence(atlas,reference)!=cells::Status::ok||
     cells::files::deserialize_world(bundle.at("initial"),atlas,initial)!=cells::Status::ok)
    return invalid("Cell Behavior model, source or initial checkpoint is invalid");
  return {};
}
std::vector<Observation> observations(const cells::World& world){
  const auto m=cells::metrics(world);
  return {{std::string(observation_schema),"viable_cells","synthetic_cell_count","count",m.viable,world.tick},
          {std::string(observation_schema),"active_links","synthetic_link_count","count",m.linked,world.tick},
          {std::string(observation_schema),"energy","model_energy_proxy","synthetic_proxy",m.total_energy,world.tick},
          {std::string(observation_schema),"learning","model_learning_proxy","synthetic_proxy",m.total_learning,world.tick}};
}
class CellEngine final:public IEngine,public ITrialCapability,public IChangeCapability {
  EngineIdentity identity_;EngineState state_;
  cells::Atlas atlas_{};cells::World world_{};
public:
  explicit CellEngine(EngineIdentity identity):identity_(std::move(identity)){}
  Result initialize(const Envelope& config,const EngineState* restored){
    Bundle bundle;cells::World initial{};
    if(auto r=unpack_cell(config,bundle,atlas_,initial);!r)return r;
    if(identity_.model_sha256!=atlas_.model_digest)return invalid("Cell Behavior model identity mismatch");
    EngineState base; base.tick=initial.tick;base.configuration=config;
    base.checkpoint={std::string(checkpoint_schema),cells::files::serialize_world(initial,atlas_)};
    return restore(restored?*restored:base);
  }
  const EngineIdentity& identity()const noexcept override{return identity_;}
  const EngineState& state()const noexcept override{return state_;}
  ITrialCapability* trials()noexcept override{return this;}
  IChangeCapability* changes()noexcept override{return this;}
  Result restore(const EngineState& next)override{
    if(!valid_base(next,config_schema,checkpoint_schema))return corrupt("invalid Cell Behavior checkpoint schema");
    Bundle bundle;cells::Atlas atlas;cells::World initial{},world{};
    if(auto r=unpack_cell(next.configuration,bundle,atlas,initial);!r)return r;
    if(identity_.model_sha256!=atlas.model_digest||
       cells::files::deserialize_world(next.checkpoint.payload,atlas,world)!=cells::Status::ok||
       world.tick!=next.tick)return corrupt("Cell Behavior checkpoint does not match its model or tick");
    atlas_=std::move(atlas);world_=world;state_=next;return {};
  }
  Result install(const EngineState& next)override{return restore(next);}
  Result fork(std::unique_ptr<IEngine>& output)const override{
    auto clone=std::make_unique<CellEngine>(identity_);
    if(auto result=clone->initialize(state_.configuration,&state_);!result)return result;
    output=std::move(clone);return {};
  }
  Result prepare_step(const StepRequest& request,PreparedTransition& output)override{
    if(auto r=guard(*this,request);!r)return r;
    if(request.input.schema!=input_schema)return invalid("Cell Behavior input schema mismatch");
    std::istringstream in(request.input.payload);std::string header,extra;
    std::int32_t support=0,stress=0,signal=0;unsigned lesson=0;
    if(!(in>>header>>support>>stress>>signal>>lesson)||in>>extra||header!="CECELLINPUT/1"||lesson>1)
      return invalid("invalid Cell Behavior input");
    cells::BoundedPositionPort spatial;cells::CellBehaviorService trial(atlas_,spatial);
    if(trial.restore(world_)!=cells::Status::ok||
       trial.step({support,stress,signal,lesson==1})!=cells::Status::ok)
      return invalid("Cell Behavior rejected input or capacity");
    EngineState target=state_;target.tick=trial.world().tick;target.phase=Phase::running;
    target.checkpoint.payload=cells::files::serialize_world(trial.world(),atlas_);
    auto transition=prepared(*this,request,std::move(target));
    transition.observations=observations(trial.world());output=std::move(transition);return {};
  }
  Result prepare_change(const StepRequest& request,PreparedTransition& output)override{
    if(auto r=guard(*this,request,true);!r)return r;
    if(request.input.schema!=change_schema)return invalid("Cell Behavior change schema mismatch");
    std::istringstream in(request.input.payload);std::string header,verb,extra;
    std::uint32_t from=0,to=0,guidance=0;
    if(!(in>>header>>verb>>from>>to)||header!="CECELLCHANGE/1"||!from||!to||from==to)
      return invalid("invalid Cell Behavior route change");
    cells::BoundedPositionPort spatial;
    optimization::DigitalRoutePolicyService policy(atlas_,spatial);
    cells::World changed{};bool existed=false;std::uint16_t previous=0;
    if(verb=="route"){
      if(!(in>>guidance)||in>>extra||guidance==0||guidance>1000)
        return invalid("route guidance must be 1..1000");
      const auto weight=static_cast<std::uint16_t>(guidance);
      const optimization::ForkOption option{{1,1,from,to,optimization::RouteKind::digital_signal,0,0,weight},weight,0};
      if(!policy.apply(world_,option,changed,existed,previous))return invalid("route is ineligible in the committed cell state");
    }else if(verb=="remove-route"){
      if(in>>extra)return invalid("unexpected route change fields");
      optimization::Candidate route{};route.id=1;route.from=from;route.to=to;route.kind=optimization::RouteKind::digital_signal;
      for(std::size_t i=0;i<world_.intent_count;++i)
        if(world_.intents[i].from==from&&world_.intents[i].to==to){existed=true;previous=world_.intents[i].guidance_weight;}
      if(!existed||!policy.undo(world_,route,false,0,changed))return invalid("route guidance does not exist");
    }else return {Code::unsupported,"unsupported Cell Behavior change"};
    EngineState target=state_;target.checkpoint.payload=cells::files::serialize_world(changed,atlas_);
    auto transition=prepared(*this,request,std::move(target));transition.observations=observations(changed);
    transition.receipts.push_back({"anaiero.cell-change-receipt/1",
      "CECELLCHANGE-RECEIPT/1\n"+verb+" "+std::to_string(from)+" "+std::to_string(to)+" "+
      std::to_string(guidance)+" "+std::to_string(existed?1:0)+" "+std::to_string(previous)+
      "\ntick "+std::to_string(state_.tick)+"\n"});
    output=std::move(transition);return {};
  }
};
class CellFactory final:public IEngineFactory {
  Result create(const EngineIdentity& identity,const Envelope& config,const EngineState* state,
                std::unique_ptr<IEngine>& output)override{
    if(identity.type_id!=cell_behavior||identity.tick_ns||identity.clock_domain!="logical")
      return invalid("Cell Behavior requires its registered type and logical clock");
    auto engine=std::make_unique<CellEngine>(identity);
    if(auto r=engine->initialize(config,state);!r)return r;
    output=std::move(engine);return {};
  }
};
}
Result add_cell(EngineModuleCatalog& catalog){
  auto d=descriptor(cell_behavior,"Cell Behavior Engine","anaiero-sw5/1",
    std::string(config_schema),std::string(input_schema),std::string(checkpoint_schema),std::string(observation_schema));
  d.isolated_trials=true;d.change_preparation=true;d.change_schemas={std::string(change_schema)};
  return catalog.add(std::move(d),std::make_shared<CellFactory>());
}
}
sdk::Result cell_configuration(std::string_view atlas,std::string_view reference,
                               std::string_view world,sdk::Envelope& output){
  sdk::Envelope candidate{"anaiero.cell-bundle/1",detail::encode({{"atlas",std::string(atlas)},
    {"reference",std::string(reference)},{"initial",std::string(world)}})};
  detail::Bundle bundle;cells::Atlas parsed;cells::World initial{};
  if(auto result=detail::unpack_cell(candidate,bundle,parsed,initial);!result)return result;
  output=std::move(candidate);return {};
}
sdk::Envelope cell_input(std::int32_t support,std::int32_t stress,std::int32_t signal,bool lesson){
  return {"anaiero.cell-input/1","CECELLINPUT/1 "+std::to_string(support)+" "+std::to_string(stress)+
    " "+std::to_string(signal)+" "+std::to_string(lesson?1:0)+"\n"};
}
}

#include "common.hpp"
#include "anaiero/services/passive_membrane_service.hpp"
#include "anaiero/services/environment_compartment_service.hpp"
#include "anaiero/infrastructure/passive_model_file.hpp"
#include "anaiero/infrastructure/environment_model_file.hpp"
#include <iomanip>
#include <locale>
#include <sstream>
namespace anaiero::engines {
namespace detail {
namespace {
constexpr std::string_view config_schema="anaiero.numerical-bundle/1";
constexpr std::string_view input_schema="anaiero.numerical-input/1";
constexpr std::string_view checkpoint_schema="anaiero.numerical-checkpoint/1";
struct NumericalData {
  Bundle bundle;std::uint64_t step=0;std::int32_t input=0;
  science::PassiveMembraneModel passive;science::EnvironmentCompartmentModel environment;
};
Result unpack_numerical(std::string_view type,const Envelope& configuration,NumericalData& output){
  NumericalData data;std::uint64_t input=0;
  if(configuration.schema!=config_schema||!decode(configuration.payload,data.bundle)||
     !has_exact(data.bundle,{"type","model","step_ns","input"})||data.bundle.at("type")!=type||
     !unsigned_number(data.bundle.at("step_ns"),data.step)||!data.step||data.step>1000000000ULL||
     !unsigned_number(data.bundle.at("input"),input)||input>1000)return invalid("invalid numerical model bundle or time/input bounds");
  data.input=static_cast<std::int32_t>(input);std::string model_sha;
  if(type==passive_membrane){
    science::PassiveMembraneService check;
    if(!science::files::parse_passive_model(data.bundle.at("model"),data.passive,model_sha)||
       !check.initialize(data.passive,data.step))return invalid("invalid passive membrane model");
  }else if(type==environment){
    science::EnvironmentCompartmentService check;
    if(!science::files::parse_environment_model(data.bundle.at("model"),data.environment,model_sha)||
       !check.initialize(data.environment,data.step))return invalid("invalid scalar environment model");
  }else return {Code::unsupported,"unknown numerical model type"};
  output=std::move(data);return {};
}
std::string checkpoint(std::uint64_t tick,double value){
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<"CENUMERICALSTATE/1\n"<<tick<<' '<<std::setprecision(17)<<value<<'\n';return out.str();
}
bool decode_checkpoint(std::string_view input,std::uint64_t& tick,double& value){
  if(input.size()>256)return false;
  std::istringstream in{std::string(input)};in.imbue(std::locale::classic());std::string magic,extra;
  return bool(in>>magic>>tick>>value)&&!(in>>extra)&&magic=="CENUMERICALSTATE/1";
}
class NumericalEngine final:public IEngine {
  EngineIdentity identity_;EngineState state_;NumericalData data_;
  science::PassiveMembraneService passive_;science::EnvironmentCompartmentService environment_;
  bool is_environment()const noexcept{return identity_.type_id==environment;}
public:
  explicit NumericalEngine(EngineIdentity identity):identity_(std::move(identity)){}
  Result initialize(const Envelope& config,const EngineState* restored){
    NumericalData data;if(auto r=unpack_numerical(identity_.type_id,config,data);!r)return r;
    if(identity_.model_sha256!=sha256(data.bundle.at("model"))||identity_.tick_ns!=data.step||
       identity_.clock_domain!="simulated_ns")return invalid("numerical model or clock identity mismatch");
    const auto initial=is_environment()?static_cast<double>(data.environment.initial_micromolar):
      static_cast<double>(data.passive.rest_microvolt);
    EngineState base;base.configuration=config;base.checkpoint={std::string(checkpoint_schema),checkpoint(0,initial)};
    return restore(restored?*restored:base);
  }
  const EngineIdentity& identity()const noexcept override{return identity_;}
  const EngineState& state()const noexcept override{return state_;}
  Result restore(const EngineState& state)override{
    if(!valid_base(state,config_schema,checkpoint_schema))return corrupt("numerical checkpoint schema mismatch");
    NumericalData data;if(auto r=unpack_numerical(identity_.type_id,state.configuration,data);!r)return r;
    std::uint64_t tick=0;double value=0;
    if(identity_.model_sha256!=sha256(data.bundle.at("model"))||identity_.tick_ns!=data.step||
       !decode_checkpoint(state.checkpoint.payload,tick,value)||tick!=state.tick||tick>1000000)
      return corrupt("numerical checkpoint model, step or sequence mismatch");
    science::PassiveMembraneService passive;science::EnvironmentCompartmentService environment;
    if(is_environment()){
      if(!environment.initialize(data.environment,data.step)||!environment.restore({tick,value}))
        return corrupt("invalid environmental concentration checkpoint");
    }else if(!passive.initialize(data.passive,data.step)||!passive.restore({tick,value}))
      return corrupt("invalid passive membrane voltage checkpoint");
    data_=std::move(data);state_=state;passive_=passive;environment_=environment;return {};
  }
  Result install(const EngineState& state)override{return restore(state);}
  Result prepare_step(const StepRequest& request,PreparedTransition& output)override{
    if(auto r=guard(*this,request);!r)return r;
    if(request.input.schema!=input_schema)return invalid("numerical input schema mismatch");
    std::int32_t input=data_.input;
    if(!request.input.payload.empty()){
      std::istringstream in(request.input.payload);std::string magic,extra;
      if(!(in>>magic>>input)||in>>extra||magic!="CENUMERICALINPUT/1"||input<0||input>1000)
        return invalid("numerical input must be 0..1000");
    }
    EngineState target=state_;target.tick=state_.tick+1;target.phase=Phase::running;
    std::int64_t sample=0;std::string metric,quantity,unit,schema;
    if(is_environment()){
      auto trial=environment_;if(!trial.advance(target.tick,input))return {Code::capacity,"environmental model rejected sequence or input"};
      target.checkpoint.payload=checkpoint(target.tick,trial.state().concentration_micromolar);
      sample=trial.concentration_micromolar();metric="concentration";quantity="generic_scalar_concentration";
      unit="micromolar";schema="anaiero.environment-observation/1";
    }else{
      auto trial=passive_;if(!trial.advance(target.tick,input))return {Code::capacity,"passive model rejected sequence or input"};
      target.checkpoint.payload=checkpoint(target.tick,trial.state().voltage_microvolt);
      sample=trial.voltage_microvolt();metric="membrane_voltage";quantity="passive_membrane_voltage";
      unit="microvolt";schema="anaiero.passive-observation/1";
    }
    auto transition=prepared(*this,request,std::move(target));
    transition.observations.push_back({std::move(schema),std::move(metric),std::move(quantity),std::move(unit),sample,transition.target.tick});
    output=std::move(transition);return {};
  }
};
class NumericalFactory final:public IEngineFactory {
  Result create(const EngineIdentity& identity,const Envelope& config,const EngineState* state,
                std::unique_ptr<IEngine>& output)override{
    auto engine=std::make_unique<NumericalEngine>(identity);
    if(auto r=engine->initialize(config,state);!r)return r;
    output=std::move(engine);return {};
  }
};
}
Result add_numerical(EngineModuleCatalog& catalog){
  auto passive=descriptor(passive_membrane,"Passive Membrane Numerical Engine",
    sha256(science::kPassiveAnalyticBehavior),std::string(config_schema),std::string(input_schema),
    std::string(checkpoint_schema),"anaiero.passive-observation/1");
  if(auto r=catalog.add(std::move(passive),std::make_shared<NumericalFactory>());!r)return r;
  auto environment_descriptor=descriptor(environment,"Scalar Environment Numerical Engine",
    sha256(science::kEnvironmentAnalyticBehavior),std::string(config_schema),std::string(input_schema),
    std::string(checkpoint_schema),"anaiero.environment-observation/1");
  return catalog.add(std::move(environment_descriptor),std::make_shared<NumericalFactory>());
}
}
sdk::Result numerical_configuration(std::string_view type,std::string_view model,
                                    std::uint64_t tick_ns,std::int32_t input,sdk::Envelope& output){
  sdk::Envelope candidate{"anaiero.numerical-bundle/1",detail::encode({{"type",std::string(type)},
    {"model",std::string(model)},{"step_ns",std::to_string(tick_ns)},{"input",std::to_string(input)}})};
  detail::NumericalData data;if(auto r=detail::unpack_numerical(type,candidate,data);!r)return r;
  output=std::move(candidate);return {};
}
sdk::Envelope numerical_input(std::int32_t value){
  return {"anaiero.numerical-input/1","CENUMERICALINPUT/1 "+std::to_string(value)+"\n"};
}
}

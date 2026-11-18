#include "common.hpp"
#include "christeye/living/colony_runner.hpp"
#include <algorithm>
#include <set>
#include <sstream>
namespace anaiero::engines {
namespace detail {
namespace {
namespace sim=christeye::living::sim3;
constexpr std::string_view config_schema="anaiero.colony-bundle/1";
constexpr std::string_view input_schema="anaiero.colony-input/1";
constexpr std::string_view change_schema="anaiero.colony-change/1";
constexpr std::string_view checkpoint_schema="CEVRUNSTATE/1";
constexpr std::string_view observation_schema="anaiero.colony-observation/1";
struct Teaching {std::string id,colony;std::uint32_t subject=0,value=0,at=0;};
struct ColonyData {Bundle bundle;sim::Configuration original,current;std::vector<Teaching> teachings;};
std::string serialize_configuration(const sim::Configuration& c){
  std::ostringstream out;out<<"CEVRUN/1\nname "<<c.name<<"\nlaw_version 1\n";
  const auto& l=c.laws;
  for(const auto& [name,value]:std::initializer_list<std::pair<const char*,std::int32_t>>{
    {"food_per_cell",l.food_per_cell},{"fed_gain",l.fed_gain},{"unfed_cost",l.unfed_cost},
    {"upkeep_cost",l.upkeep_cost},{"stress_cutoff",l.stress_cutoff},{"stress_cost",l.stress_cost},
    {"maturity_gain",l.maturity_gain},{"blood_step",l.blood_step},{"learning_gain",l.learning_gain}})
    out<<"law "<<name<<' '<<value<<'\n';
  out<<"source "<<c.material_role<<' '<<c.material_path<<' '<<c.material_sha<<'\n';
  for(const auto& colony:c.colonies)out<<"colony "<<colony.id<<' '<<colony.source_path<<' '<<colony.source_sha<<'\n';
  for(const auto& seed:c.seeds)out<<"seed "<<seed.colony<<' '<<seed.id<<' '<<seed.kind<<' '
    <<seed.x<<' '<<seed.y<<' '<<seed.z<<' '<<seed.parent<<'\n';
  for(const auto& link:c.links)out<<"link "<<link.from<<' '<<link.to<<' '<<sim::channel_text(link.channel)<<' '<<link.units<<'\n';
  for(const auto& p:c.processes)out<<"process "<<p.colony<<' '<<sim::process_text(p.kind)<<' '
    <<p.subject<<' '<<p.value<<' '<<p.at<<'\n';
  out<<"end\n";return out.str();
}
std::string encode_teachings(const std::vector<Teaching>& values){
  std::ostringstream out;out<<"CETEACHINGS/1\n";
  for(const auto& t:values)out<<t.id<<' '<<t.colony<<' '<<t.subject<<' '<<t.value<<' '<<t.at<<'\n';
  return out.str();
}
bool same_process(const sim::Process& p,const Teaching& t){
  return p.kind==sim::ProcessKind::teach&&p.colony==t.colony&&p.subject==t.subject&&p.value==t.value&&p.at==t.at;
}
Result unpack_colony(const Envelope& configuration,ColonyData& output){
  ColonyData data;
  if(configuration.schema!=config_schema||!decode(configuration.payload,data.bundle)||
     !data.bundle.contains("model")||!data.bundle.contains("current")||!data.bundle.contains("teachings")||
     sim::Configuration::parse(data.bundle.at("model"),data.original)!=sim::Result::ok||
     sim::Configuration::parse(data.bundle.at("current"),data.current)!=sim::Result::ok)
    return invalid("invalid retained Multi-Colony configuration");
  std::map<std::string,std::string> sources{{data.original.material_path,data.original.material_sha}};
  for(const auto& colony:data.original.colonies){
    const auto [it,inserted]=sources.emplace(colony.source_path,colony.source_sha);
    if(!inserted&&it->second!=colony.source_sha)return invalid("inconsistent retained Multi-Colony source");
  }
  if(data.bundle.size()!=sources.size()+3)return invalid("unexpected Multi-Colony source assets");
  for(const auto& [path,digest]:sources){
    const auto it=data.bundle.find("asset/"+path);
    if(it==data.bundle.end()||sha256(it->second)!=digest)return invalid("retained Multi-Colony source digest mismatch");
    if(path==data.original.material_path&&(!it->second.starts_with("CEVSOURCE/1\n")||
      it->second.find("\nsource "+data.original.material_role+"\n")==std::string::npos))
      return invalid("Multi-Colony source provenance role mismatch");
  }
  // Supported changes affect links and teaching schedules only. The model,
  // initial cells, laws and provenance remain the pinned original definition.
  auto invariant=data.current;invariant.links=data.original.links;invariant.processes=data.original.processes;
  if(serialize_configuration(invariant)!=serialize_configuration(data.original))
    return invalid("unsupported Multi-Colony model or source change");
  std::istringstream in(data.bundle.at("teachings"));std::string header,line;
  if(!std::getline(in,header)||header!="CETEACHINGS/1")return invalid("invalid teaching schedule");
  std::set<std::string> ids;
  while(std::getline(in,line)){
    std::istringstream row(line);Teaching t;std::string extra;
    if(!(row>>t.id>>t.colony>>t.subject>>t.value>>t.at)||row>>extra||!valid_token(t.id,64)||
       !ids.insert(t.id).second||data.teachings.size()>=64||!t.at||t.at>10000||t.value>100||
       std::count_if(data.current.processes.begin(),data.current.processes.end(),[&](const auto& p){return same_process(p,t);})!=1)
      return invalid("invalid or ambiguous teaching schedule");
    data.teachings.push_back(std::move(t));
  }
  if(data.current.links.size()!=data.original.links.size())return invalid("unsupported link topology change");
  for(std::size_t i=0;i<data.current.links.size();++i){
    const auto& a=data.current.links[i];const auto& b=data.original.links[i];
    if(a.from!=b.from||a.to!=b.to||a.channel!=b.channel)return invalid("unsupported link topology change");
  }
  auto expected=data.original;expected.links=data.current.links;
  for(const auto& t:data.teachings)
    expected.processes.push_back({t.colony,sim::ProcessKind::teach,t.subject,t.value,t.at});
  if(serialize_configuration(expected)!=serialize_configuration(data.current))
    return invalid("teaching records do not account for the configured process changes");
  output=std::move(data);return {};
}
std::vector<Observation> observations(const sim::Snapshot& state,const sim::Configuration& config){
  std::int64_t viable=0,energy=0,learning=0;
  for(const auto& cell:state.cells){viable+=cell.alive?1:0;energy+=cell.energy;learning+=cell.learning;}
  return {{std::string(observation_schema),"viable_cells","synthetic_cell_count","count",viable,state.tick},
    {std::string(observation_schema),"configured_links","synthetic_colony_link_count","count",static_cast<std::int64_t>(config.links.size()),state.tick},
    {std::string(observation_schema),"energy","model_energy_proxy","synthetic_proxy",energy,state.tick},
    {std::string(observation_schema),"learning","model_learning_proxy","synthetic_proxy",learning,state.tick}};
}
bool valid_snapshot(const sim::Snapshot& value){
  if(value.tick>10000)return false;
  std::set<std::string> colonies;
  for(const auto& c:value.colonies){
    colonies.insert(c.id);
    for(const auto quantity:{c.food,c.data,c.signal,c.stress})if(quantity<0||quantity>100)return false;
  }
  for(const auto& cell:value.cells)
    if(!colonies.contains(cell.colony)||cell.maturity<0||cell.maturity>100||cell.learning<0||cell.learning>100||
       !(cell.kind=="crest_progenitor"||cell.kind=="blood_carrier"||cell.kind=="neural_progenitor"||
         cell.kind=="neuron"||cell.kind=="astrocyte"))return false;
  return true;
}
class ColonyEngine final:public IEngine,public ITrialCapability,public IChangeCapability {
  EngineIdentity identity_;EngineState state_;ColonyData data_;sim::Snapshot world_;
public:
  explicit ColonyEngine(EngineIdentity identity):identity_(std::move(identity)){}
  Result initialize(const Envelope& config,const EngineState* restored){
    ColonyData data;if(auto r=unpack_colony(config,data);!r)return r;
    if(identity_.model_sha256!=data.original.hash)return invalid("Multi-Colony model identity mismatch");
    sim::Snapshot initial;initial.config_hash=data.current.hash;
    for(const auto& colony:data.current.colonies)initial.colonies.push_back({colony.id});
    for(const auto& seed:data.current.seeds)
      initial.cells.push_back({seed.colony,seed.kind,seed.id,seed.parent,seed.x,seed.y,seed.z});
    EngineState base;base.configuration=config;base.checkpoint={std::string(checkpoint_schema),sim::encode_snapshot(initial)};
    return restore(restored?*restored:base);
  }
  const EngineIdentity& identity()const noexcept override{return identity_;}
  const EngineState& state()const noexcept override{return state_;}
  ITrialCapability* trials()noexcept override{return this;}
  IChangeCapability* changes()noexcept override{return this;}
  Result restore(const EngineState& state)override{
    if(!valid_base(state,config_schema,checkpoint_schema))return corrupt("Multi-Colony checkpoint schema mismatch");
    ColonyData data;if(auto r=unpack_colony(state.configuration,data);!r)return r;
    sim::Snapshot world;
    if(identity_.model_sha256!=data.original.hash||
       sim::decode_snapshot(state.checkpoint.payload,data.current,world)!=sim::Result::ok||
       world.tick!=state.tick||!valid_snapshot(world))return corrupt("Multi-Colony state/configuration identity mismatch");
    data_=std::move(data);world_=std::move(world);state_=state;return {};
  }
  Result install(const EngineState& state)override{return restore(state);}
  Result fork(std::unique_ptr<IEngine>& output)const override{
    auto clone=std::make_unique<ColonyEngine>(identity_);
    if(auto r=clone->initialize(state_.configuration,&state_);!r)return r;
    output=std::move(clone);return {};
  }
  Result prepare_step(const StepRequest& request,PreparedTransition& output)override{
    if(auto r=guard(*this,request);!r)return r;
    if(request.input.schema!=input_schema||!request.input.payload.empty())
      return invalid("Multi-Colony uses its own configured processes; step payload must be empty");
    auto world=world_;simulation::Sim3TransitionService transitions;
    if(transitions.advance(data_.current,world)!=sim::Result::ok)return {Code::capacity,"Multi-Colony tick/cell capacity exhausted"};
    EngineState target=state_;target.tick=world.tick;target.phase=Phase::running;
    target.checkpoint.payload=sim::encode_snapshot(world);
    auto transition=prepared(*this,request,std::move(target));transition.observations=observations(world,data_.current);
    for(const auto& teaching:data_.teachings)if(teaching.at==world.tick){
      std::int32_t before=0,after=0;bool applied=false;
      for(const auto& c:world_.cells)if(c.colony==teaching.colony&&c.id==teaching.subject){before=c.learning;applied=c.alive;}
      for(const auto& c:world.cells)if(c.colony==teaching.colony&&c.id==teaching.subject)after=c.learning;
      transition.receipts.push_back({"anaiero.teaching-executed/1",
        "CETEACH-EXECUTED/1\n"+teaching.id+" "+std::to_string(teaching.at)+" "+
        std::to_string(before)+" "+std::to_string(after)+" "+std::to_string(applied?1:0)+"\n"});
    }
    output=std::move(transition);return {};
  }
  Result prepare_change(const StepRequest& request,PreparedTransition& output)override{
    if(auto r=guard(*this,request,true);!r)return r;
    if(request.input.schema!=change_schema)return invalid("Multi-Colony change schema mismatch");
    std::istringstream in(request.input.payload);std::string header,verb,extra;
    if(!(in>>header>>verb)||header!="CECOLONYCHANGE/1")return invalid("invalid Multi-Colony change");
    auto data=data_;std::string receipt="CECOLONYCHANGE-RECEIPT/1\n";
    if(verb=="link"){
      std::string from,to,channel;std::uint32_t units=0;sim::Channel kind{};
      if(!(in>>from>>to>>channel>>units)||in>>extra||units>100||!sim::channel_from(channel,kind))
        return invalid("invalid Multi-Colony link policy");
      sim::Link* selected=nullptr;
      for(auto& link:data.current.links)if(link.from==from&&link.to==to&&link.channel==kind){
        if(selected)return invalid("parallel links require a distinct model change");
        selected=&link;
      }
      if(!selected)return {Code::not_found,"configured Multi-Colony link not found"};
      receipt+="link "+from+" "+to+" "+channel+" "+std::to_string(selected->units)+" "+std::to_string(units)+"\n";
      selected->units=units;
    }else if(verb=="teach"){
      Teaching teaching;
      if(!(in>>teaching.id>>teaching.colony>>teaching.subject>>teaching.value>>teaching.at)||in>>extra||
         !valid_token(teaching.id,64)||teaching.value>100||teaching.at<=state_.tick||teaching.at>10000)
        return invalid("teaching must name a unique ID and future tick within 10000");
      if(data.current.processes.size()>=sim::kMaxProcesses||data.teachings.size()>=64)
        return {Code::capacity,"Multi-Colony teaching schedule capacity reached"};
      for(const auto& t:data.teachings)if(t.id==teaching.id)return {Code::conflict,"teaching ID already exists"};
      if(std::none_of(world_.cells.begin(),world_.cells.end(),[&](const auto& c){
          return c.colony==teaching.colony&&c.id==teaching.subject&&c.alive;}))return invalid("teaching target is not a live cell");
      for(const auto& p:data.current.processes)if(same_process(p,teaching))return {Code::conflict,"teaching would duplicate an existing process"};
      data.current.processes.push_back({teaching.colony,sim::ProcessKind::teach,teaching.subject,teaching.value,teaching.at});
      data.teachings.push_back(teaching);receipt+="scheduled "+teaching.id+" "+std::to_string(teaching.at)+"\n";
    }else if(verb=="cancel-teach"){
      std::string id;if(!(in>>id)||in>>extra)return invalid("invalid teaching cancellation");
      const auto found=std::find_if(data.teachings.begin(),data.teachings.end(),[&](const auto& t){return t.id==id;});
      if(found==data.teachings.end())return {Code::not_found,"teaching ID not found"};
      if(found->at<=state_.tick)return {Code::conflict,"teaching has executed; its effect and receipt cannot be undone by configuration rollback"};
      const auto process=std::find_if(data.current.processes.begin(),data.current.processes.end(),[&](const auto& p){return same_process(p,*found);});
      if(process==data.current.processes.end())return corrupt("teaching schedule is inconsistent");
      data.current.processes.erase(process);data.teachings.erase(found);receipt+="cancelled "+id+"\n";
    }else return {Code::unsupported,"unsupported Multi-Colony change"};
    data.bundle["current"]=serialize_configuration(data.current);
    data.bundle["teachings"]=encode_teachings(data.teachings);
    EngineState target=state_;target.configuration.payload=encode(data.bundle);
    auto world=world_;world.config_hash=sha256(data.bundle.at("current"));target.checkpoint.payload=sim::encode_snapshot(world);
    ColonyData verified;
    if(auto r=unpack_colony(target.configuration,verified);!r)return r;
    sim::Snapshot validated;
    if(sim::decode_snapshot(target.checkpoint.payload,verified.current,validated)!=sim::Result::ok)
      return invalid("prepared Multi-Colony configuration cannot restore its checkpoint");
    auto transition=prepared(*this,request,std::move(target));transition.observations=observations(world,data.current);
    receipt+="tick "+std::to_string(state_.tick)+"\nconfiguration "+world.config_hash+"\n";
    transition.receipts.push_back({"anaiero.colony-change-receipt/1",std::move(receipt)});
    output=std::move(transition);return {};
  }
};
class ColonyFactory final:public IEngineFactory {
  Result create(const EngineIdentity& identity,const Envelope& config,const EngineState* state,
                std::unique_ptr<IEngine>& output)override{
    if(identity.type_id!=multi_colony||identity.tick_ns||identity.clock_domain!="logical")
      return invalid("Multi-Colony requires its registered type and logical clock");
    auto engine=std::make_unique<ColonyEngine>(identity);
    if(auto r=engine->initialize(config,state);!r)return r;
    output=std::move(engine);return {};
  }
};
}
Result add_colony(EngineModuleCatalog& catalog){
  auto d=descriptor(multi_colony,"Multi-Colony Engine","anaiero-sim3/2",
    std::string(config_schema),std::string(input_schema),std::string(checkpoint_schema),std::string(observation_schema));
  d.isolated_trials=true;d.change_preparation=true;d.change_schemas={std::string(change_schema)};
  return catalog.add(std::move(d),std::make_shared<ColonyFactory>());
}
}
sdk::Result colony_configuration(std::string_view model,std::span<const RetainedAsset> assets,sdk::Envelope& output){
  detail::Bundle bundle{{"model",std::string(model)},{"current",std::string(model)},{"teachings","CETEACHINGS/1\n"}};
  for(const auto& asset:assets)
    if(!bundle.emplace("asset/"+asset.relative_path,asset.bytes).second)return detail::invalid("duplicate Multi-Colony source asset");
  sdk::Envelope candidate{"anaiero.colony-bundle/1",detail::encode(bundle)};
  detail::ColonyData data;if(auto r=detail::unpack_colony(candidate,data);!r)return r;
  output=std::move(candidate);return {};
}
}

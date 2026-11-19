#include "anaiero/sdk/catalog.hpp"
#include <algorithm>
#include <exception>
#include <utility>

namespace anaiero::sdk {
namespace {
bool schemas(const std::vector<std::string>& values,bool required=true) {
  if((required&&values.empty())||values.size()>16)return false;
  for(std::size_t i=0;i<values.size();++i){if(!valid_token(values[i]))return false;
    for(std::size_t j=0;j<i;++j)if(values[j]==values[i])return false;}
  return true;
}
bool has(const std::vector<std::string>& v,std::string_view s) {return std::find(v.begin(),v.end(),s)!=v.end();}
class OwnedEngine final:public IEngine,public ITrialCapability,public IChangeCapability {
  // Owner (and any native image) outlives the engine and its virtual destructor.
  std::shared_ptr<IEngineFactory> owner_;
  std::unique_ptr<IEngine> inner_;
public:
  OwnedEngine(std::shared_ptr<IEngineFactory> owner,std::unique_ptr<IEngine> inner):owner_(std::move(owner)),inner_(std::move(inner)){}
  const EngineIdentity& identity()const noexcept override{return inner_->identity();}
  const EngineState& state()const noexcept override{return inner_->state();}
  Result prepare_step(const StepRequest& r,PreparedTransition& t)override{return inner_->prepare_step(r,t);}
  Result install(const EngineState& s)override{return inner_->install(s);}
  Result restore(const EngineState& s)override{return inner_->restore(s);}
  ITrialCapability* trials()noexcept override{return inner_->trials()?this:nullptr;}
  IChangeCapability* changes()noexcept override{return inner_->changes()?this:nullptr;}
  void request_cancel()noexcept override{inner_->request_cancel();}
  Result fork(std::unique_ptr<IEngine>& out)const override{
    auto* capability=inner_->trials();if(!capability)return {Code::unsupported,"isolated trials unavailable"};
    std::unique_ptr<IEngine> next;auto r=capability->fork(next);if(!r)return r;
    if(!next||next->identity()!=identity()||next->state()!=state())return {Code::corrupt,"fork changed source identity/state"};
    out=std::make_unique<OwnedEngine>(owner_,std::move(next));return {};
  }
  Result prepare_change(const StepRequest& r,PreparedTransition& t)override{
    auto* capability=inner_->changes();return capability?capability->prepare_change(r,t):Result{Code::unsupported,"change unavailable"};
  }
};
}
struct EngineModuleCatalog::Impl {
  struct Entry {EngineDescriptor descriptor;std::shared_ptr<IEngineFactory> factory;};
  std::size_t capacity;bool frozen=false;std::vector<Entry> entries;
  std::vector<std::pair<Digest,Digest>> compatible;
  explicit Impl(std::size_t value):capacity(value){}
};
EngineModuleCatalog::EngineModuleCatalog(std::size_t capacity):impl_(std::make_unique<Impl>(capacity)){}
EngineModuleCatalog::~EngineModuleCatalog()=default;
Result EngineModuleCatalog::allow_retained_module(std::string_view retained,std::string_view executing){
  if(impl_->frozen||!valid_digest(retained)||!valid_digest(executing)||retained==executing)
    return {Code::invalid,"invalid verified compatibility binding"};
  if(impl_->compatible.size()==16)return {Code::capacity,"compatibility binding capacity"};
  impl_->compatible.emplace_back(retained,executing);return {};
}
Result EngineModuleCatalog::add(EngineDescriptor d,std::shared_ptr<IEngineFactory> f){
  if(impl_->frozen)return {Code::conflict,"catalog is frozen"};
  if(!f||!valid_token(d.type_id)||d.display_name.empty()||d.display_name.size()>128||!valid_token(d.behavior_id)||
     !valid_digest(d.module_sha256)||!schemas(d.configuration_schemas)||!schemas(d.input_schemas)||
     !schemas(d.checkpoint_schemas)||!schemas(d.observation_schemas,false)||!schemas(d.change_schemas,false)||
     d.change_preparation!=!d.change_schemas.empty()||!d.limits.max_payload_bytes||d.limits.max_payload_bytes>4U*1024U*1024U||
     !d.limits.max_checkpoint_bytes||d.limits.max_checkpoint_bytes>4U*1024U*1024U||
     d.limits.max_observations>128||d.limits.max_receipts>128)return {Code::invalid,"module descriptor or limits"};
  for(const auto& schema:d.change_schemas)
    if(has(d.input_schemas,schema))return {Code::invalid,"step and change schemas must identify distinct operations"};
  if(find(d.type_id))return {Code::conflict,"duplicate engine type"};
  if(!impl_->capacity||impl_->capacity>256||impl_->entries.size()==impl_->capacity)return {Code::capacity,"catalog capacity"};
  impl_->entries.push_back({std::move(d),std::move(f)});return {};
}
Result EngineModuleCatalog::freeze(){if(impl_->entries.empty())return {Code::invalid,"empty catalog"};impl_->frozen=true;return {};}
bool EngineModuleCatalog::frozen()const noexcept{return impl_->frozen;}
std::size_t EngineModuleCatalog::size()const noexcept{return impl_->entries.size();}
const EngineDescriptor* EngineModuleCatalog::find(std::string_view id)const noexcept{
  for(const auto& e:impl_->entries)if(e.descriptor.type_id==id)return &e.descriptor;
  return nullptr;
}
std::vector<EngineDescriptor> EngineModuleCatalog::describe()const{
  std::vector<EngineDescriptor> out;for(const auto& e:impl_->entries)out.push_back(e.descriptor);return out;
}
Result EngineModuleCatalog::create(const EngineIdentity& id,const Envelope& configuration,const EngineState* saved,
                                 std::unique_ptr<IEngine>& output){
  if(!impl_->frozen)return {Code::conflict,"catalog must be frozen before activation"};
  if(!valid_identity(id))return {Code::invalid,"invalid engine identity"};
  for(const auto& e:impl_->entries){const auto& d=e.descriptor;if(d.type_id!=id.type_id)continue;
    const bool compatible=std::find(impl_->compatible.begin(),impl_->compatible.end(),
      std::pair<Digest,Digest>{id.module_sha256,d.module_sha256})!=impl_->compatible.end();
    if((id.module_sha256!=d.module_sha256&&!compatible)||id.behavior_id!=d.behavior_id)return {Code::conflict,"pinned module/behavior mismatch"};
    if(!has(d.configuration_schemas,configuration.schema)||configuration.payload.size()>d.limits.max_payload_bytes)
      return {Code::unsupported,"configuration schema/bounds"};
    if(saved&&(!valid_state(*saved,d.limits)||saved->configuration!=configuration||!has(d.checkpoint_schemas,saved->checkpoint.schema)))
      return {Code::corrupt,"restored engine contract"};
    try{
      std::unique_ptr<IEngine> next;auto result=e.factory->create(id,configuration,saved,next);if(!result)return result;
      if(!next||next->identity()!=id||!valid_state(next->state(),d.limits)||next->state().configuration!=configuration||
         !has(d.checkpoint_schemas,next->state().checkpoint.schema)||
         (next->trials()!=nullptr)!=d.isolated_trials||(next->changes()!=nullptr)!=d.change_preparation||
         (saved&&next->state()!=*saved))return {Code::corrupt,"module capability/state conformance"};
      output=std::make_unique<OwnedEngine>(e.factory,std::move(next));return {};
    }catch(const std::exception& error){return {Code::io_error,std::string("engine construction: ")+error.what()};}
    catch(...){return {Code::io_error,"engine construction threw"};}
  }
  return {Code::unsupported,"engine type is not registered"};
}
}

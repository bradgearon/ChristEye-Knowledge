#include "anaiero/sdk/projection.hpp"
#include <algorithm>
#include <tuple>

namespace anaiero::sdk {
namespace {
bool same_key(const MetricView& view,const EngineIdentity& id,const Observation& observation) {
  return view.identity.source_uid==id.source_uid&&view.identity.epoch==id.epoch&&
    view.observation.metric==observation.metric;
}
}
struct ProjectionService::Impl {
  std::size_t capacity;
  std::vector<MetricView> values;
  explicit Impl(std::size_t value):capacity(value){}
};
ProjectionService::ProjectionService(std::size_t capacity):impl_(std::make_unique<Impl>(capacity)){}
ProjectionService::~ProjectionService()=default;
Result ProjectionService::can_apply(const PreparedTransition& transition)const {
  if(!valid_identity(transition.identity))return {Code::invalid,"projection source identity"};
  std::size_t additions=0;
  for(std::size_t i=0;i<transition.observations.size();++i) {
    const auto& o=transition.observations[i];
    if(!valid_token(o.schema)||!valid_token(o.metric)||!valid_token(o.quantity)||!valid_token(o.unit)||
        o.tick>transition.target.tick)return {Code::invalid,"projection observation identity/clock"};
    for(std::size_t j=0;j<i;++j)
      if(transition.observations[j].metric==o.metric)return {Code::conflict,"duplicate metric in projection"};
    const auto at=std::find_if(impl_->values.begin(),impl_->values.end(),[&](const auto& value){
      return same_key(value,transition.identity,o);
    });
    if(at==impl_->values.end())++additions;
    else if(at->identity!=transition.identity||at->observation.schema!=o.schema||
        at->observation.quantity!=o.quantity||at->observation.unit!=o.unit)
      return {Code::conflict,"metric identity, quantity or unit changed within a source epoch"};
  }
  if(impl_->capacity>65536||additions>impl_->capacity||impl_->values.size()>impl_->capacity-additions)
    return {Code::capacity,"projection active metric capacity"};
  return {};
}
Result ProjectionService::apply(const PreparedTransition& transition,std::uint64_t generation) {
  auto result=can_apply(transition);if(!result)return result;
  if(!generation)return {Code::invalid,"projection requires a committed generation"};
  auto next=impl_->values;
  for(const auto& o:transition.observations) {
    auto at=std::find_if(next.begin(),next.end(),[&](const auto& value){return same_key(value,transition.identity,o);});
    if(at==next.end())next.push_back({transition.identity,o,transition.target.revision,generation});
    else {
      if(generation<at->generation||transition.target.revision<at->revision)
        return {Code::conflict,"projection refused stale committed evidence"};
      if(generation==at->generation&&(at->revision!=transition.target.revision||at->observation!=o))
        return {Code::conflict,"projection generation equivocation"};
      *at={transition.identity,o,transition.target.revision,generation};
    }
  }
  impl_->values=std::move(next);return {};
}
void ProjectionService::retire(std::string_view source_uid,std::uint64_t epoch) {
  std::erase_if(impl_->values,[&](const auto& value){return value.identity.source_uid==source_uid&&value.identity.epoch==epoch;});
}
Result ProjectionService::replace(const std::vector<MetricView>& committed) {
  ProjectionService staged(impl_->capacity);
  for(const auto& value:committed) {
    PreparedTransition transition;transition.identity=value.identity;
    transition.target.revision=value.revision;transition.target.tick=value.observation.tick;
    transition.observations.push_back(value.observation);
    auto result=staged.apply(transition,value.generation);if(!result)return result;
  }
  impl_->values=std::move(staged.impl_->values);return {};
}
void ProjectionService::clear(){impl_->values.clear();}
std::vector<MetricView> ProjectionService::snapshot()const {
  auto out=impl_->values;
  std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){
    return std::tie(a.identity.source_uid,a.identity.epoch,a.observation.metric)<
      std::tie(b.identity.source_uid,b.identity.epoch,b.observation.metric);
  });
  return out;
}
std::size_t ProjectionService::size()const noexcept{return impl_->values.size();}
}

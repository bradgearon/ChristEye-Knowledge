#pragma once
#include "anaiero/contracts/service_extension.hpp"
#include <array>
#include <cstddef>
namespace anaiero::services {
template<std::size_t Capacity=4>
class ServiceExtensionDispatcher final:public contracts::IServiceExtension {
  static_assert(Capacity>0&&Capacity<=16);
  std::array<contracts::IServiceExtension*,Capacity> hooks_{};
  std::size_t count_=0;
public:
  bool add(contracts::IServiceExtension& hook) noexcept {
    if(count_==Capacity)return false;
    for(std::size_t i=0;i<count_;++i)if(hooks_[i]==&hook)return false;
    hooks_[count_++]=&hook;return true;
  }
  void on_service_event(const contracts::ServiceEvent& e) noexcept override {
    for(std::size_t i=0;i<count_;++i)hooks_[i]->on_service_event(e);
  }
  std::size_t size()const noexcept{return count_;}
};
} // namespace anaiero::services

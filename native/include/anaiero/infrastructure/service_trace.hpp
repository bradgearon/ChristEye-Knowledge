#pragma once
// Bounded diagnostics only. These observations never substitute for receipts.
#include "anaiero/contracts/service_extension.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
namespace anaiero::infrastructure {
class ServiceTrace final:public contracts::IServiceExtension,public contracts::ITraceView {
  struct Item {std::uint64_t sequence=0;contracts::ServiceEvent event{};};
  mutable std::mutex mutex_;
  std::array<Item,256> items_{};
  std::uint64_t sequence_=0;
public:
  void on_service_event(const contracts::ServiceEvent& event)noexcept override{
    std::lock_guard<std::mutex> lock(mutex_);
    if(sequence_==UINT64_MAX)return;
    ++sequence_;items_[(sequence_-1)%items_.size()]={sequence_,event};
  }
  std::string trace_json()const override{
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream out;out<<"{\"schema\":\"anaiero.trace/1\",\"durable\":false,\"sequence\":"<<sequence_
      <<",\"retained\":"<<(sequence_<items_.size()?sequence_:items_.size())<<",\"events\":[";
    const auto first=sequence_>32?sequence_-31:1;
    for(auto n=first;n<=sequence_&&n;++n){
      if(n!=first)out<<',';
      const auto& e=items_[(n-1)%items_.size()].event;
      out<<"{\"sequence\":"<<n<<",\"service\":"<<static_cast<unsigned>(e.service)
        <<",\"phase\":"<<static_cast<unsigned>(e.phase)<<",\"revision\":"<<e.revision
        <<",\"subject\":"<<e.subject<<",\"status\":"<<e.status<<",\"operation_id\":"<<e.operation_id
        <<",\"source\":"<<e.source<<",\"epoch\":"<<e.source_epoch<<'}';
    }
    out<<"]}\n";return out.str();
  }
};
} // namespace anaiero::infrastructure

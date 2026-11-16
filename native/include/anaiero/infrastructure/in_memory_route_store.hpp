#pragma once
// In-process RAM-only digital route store, intentionally NOT crash-durable.
#include "anaiero/contracts/optimization.hpp"
#include <array>
namespace anaiero::optimization {
class InMemoryDigitalRouteStore final : public IRouteChangePort {
  struct Applied {std::uint64_t id=0;Candidate candidate{};bool active=false;};
  std::array<Applied,16> changes_{};
  std::size_t count_=0;
  std::uint64_t next_=1,revision_=0;
public:
  explicit InMemoryDigitalRouteStore(std::uint64_t initial_revision) noexcept:
    revision_(initial_revision){}
  bool commit(const Decision& d,std::uint64_t& token) noexcept override {
    token=0;
    if(count_>=changes_.size()||d.snapshot_revision!=revision_||
       d.status!=Status::ok||d.score<=0||d.selected.id==0)return false;
    token=next_++;
    changes_[count_++]={token,d.selected,true};
    ++revision_;
    return true;
  }
  bool revert(std::uint64_t id) noexcept override {
    for(std::size_t i=0;i<count_;++i)
      if(changes_[i].id==id&&changes_[i].active){
        changes_[i].active=false;++revision_;return true;
      }
    return false;
  }
  bool active(std::uint32_t route_id)const noexcept {
    for(std::size_t i=0;i<count_;++i)
      if(changes_[i].candidate.id==route_id&&changes_[i].active)return true;
    return false;
  }
  std::uint64_t revision()const noexcept{return revision_;}
};
} // namespace anaiero::optimization

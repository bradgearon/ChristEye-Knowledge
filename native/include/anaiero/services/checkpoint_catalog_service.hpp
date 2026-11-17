#pragma once
#include "anaiero/contracts/checkpoint_catalog.hpp"
#include <string_view>
namespace anaiero::engine {
class CheckpointCatalogService final {
  ICheckpointCatalog& index_;
  static bool sha(std::string_view h)noexcept {
    if(h.size()!=64)return false;
    for(char c:h)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
  }
public:
  explicit CheckpointCatalogService(ICheckpointCatalog& index)noexcept:index_(index){}
  bool commit(const CheckpointRecord& r,std::string_view snapshot) noexcept {
    return r.engine&&r.source&&r.epoch&&r.revision&&
      sha(r.model_sha256)&&sha(r.checkpoint_sha256)&&
      !snapshot.empty()&&index_.commit(r,snapshot);
  }
  bool restore(std::uint32_t id,std::uint64_t epoch,
    CheckpointRecord& r,std::string& checkpoint) noexcept {
    return id&&epoch&&index_.latest(id,epoch,r,checkpoint)&&
      r.engine==id&&r.epoch==epoch&&sha(r.checkpoint_sha256)&&
      sha(r.model_sha256);
  }
};
} // namespace anaiero::engine

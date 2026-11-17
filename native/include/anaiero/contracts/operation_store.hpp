#pragma once
#include "anaiero/contracts/native_session.hpp"
#include "anaiero/contracts/operation.hpp"
namespace anaiero::operations {
struct Item {Entry receipt{};session::Command command{};};
struct Ledger {
  std::uint64_t revision=0,next_id=1,expired_through=0,active_id=0;
  std::size_t count=0;
  std::array<Item,kRetainedOperations> items{};
};
struct IStore {
  virtual ~IStore()=default;
  virtual bool load(Ledger&,bool& exists)noexcept=0;
  virtual bool save(Ledger&)noexcept=0;
};
} // namespace anaiero::operations

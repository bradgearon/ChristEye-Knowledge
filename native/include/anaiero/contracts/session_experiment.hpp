#pragma once
// Session-facing experiment lifecycle; durable model changes stay in session.
#include "anaiero/contracts/experiment.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include <cstdint>
#include <string_view>
namespace anaiero::session {
struct IExperimentRuntime {
  virtual ~IExperimentRuntime()=default;
  virtual bool start(std::uint64_t operation_id,std::string_view reference)noexcept=0;
  virtual bool recover(std::uint64_t operation_id)noexcept=0;
  virtual bool pump()noexcept=0;
  virtual bool cancel()noexcept=0;
  virtual bool resume()noexcept=0;
  virtual bool busy()const noexcept=0;
  virtual bool failed()const noexcept=0;
  virtual const experiments::Record* record()const noexcept=0;
  virtual experiments::Code code()const noexcept=0;
  virtual bool mark_applied(std::uint64_t change_id,std::uint64_t tick)noexcept=0;
  virtual bool observe(const engine::Snapshot&)noexcept=0;
  virtual void set_extension(contracts::IServiceExtension*)noexcept{}
};
} // namespace anaiero::session

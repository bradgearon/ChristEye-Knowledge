#pragma once
#include "anaiero/contracts/native_session.hpp"
#include <cstdint>
#include <string>
namespace anaiero::session {
struct IControl {
  virtual ~IControl()=default;
  virtual bool enqueue(const Command&,std::uint64_t& ticket)noexcept=0;
  virtual bool set_running(bool)noexcept=0;
  virtual void cancel()noexcept=0;
  virtual void request_stop()noexcept=0;
  // A bounded revision watch returns the latest complete immutable view.
  virtual std::string snapshot(std::uint64_t after=0,std::uint32_t wait_ms=0)=0;
  virtual std::string operation_result(std::uint64_t){return "{\"error\":\"unsupported\"}\n";}
  virtual std::string trace(){return "{\"events\":[]}\n";}
  virtual std::string experiment_result(){return "{}\n";}
};
} // namespace anaiero::session

#pragma once
// Owned local child process. Arguments are passed directly, never through a shell.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace anaiero::process {
inline constexpr std::size_t kMaxFrameBytes=16384;
inline constexpr std::size_t kMaxDiagnosticBytes=8192;
enum class Code : std::uint8_t {ok,invalid,start_failed,io_failed,deadline,cancelled,exited,frame_limit};
struct Specification {
  std::string executable;
  std::vector<std::string> arguments;
};
struct IProcessExchange {
  virtual ~IProcessExchange()=default;
  virtual Code start(const Specification&)=0;
  // One bounded request and response line under one absolute deadline.
  virtual Code exchange(std::string_view,std::string&,std::uint32_t timeout_ms)noexcept=0;
  virtual void cancel()noexcept=0;
  virtual void stop()noexcept=0;
  virtual bool running()const noexcept=0;
  virtual Code status()const noexcept=0;
  virtual std::string diagnostics()const=0;
};
} // namespace anaiero::process

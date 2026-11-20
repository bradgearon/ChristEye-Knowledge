#pragma once
#include "anaiero/sdk/catalog.hpp"
#include "anaiero/sdk/module_abi.h"

namespace anaiero::sdk {
struct ModuleAdmission {
  std::string artifact_path;
  Digest artifact_sha256;
  // Required only for an in-process image. Hash agreement is not a trust grant.
  bool trusted_native=false;
  std::vector<std::string> arguments;
  std::uint32_t timeout_ms=2000;
  std::size_t maximum_wire_bytes=16U*1024U*1024U;
};
// Native images and owned processes retain their implementation resources until
// every engine/fork is destroyed. Both check executable bytes before admission.
Result load_native_module(const ModuleAdmission&,EngineDescriptor&,std::shared_ptr<IEngineFactory>&);
Result load_process_module(const ModuleAdmission&,EngineDescriptor&,std::shared_ptr<IEngineFactory>&);
// Adapter for module authors. It translates compiled C++ capabilities into the
// C table locally; the table/context must outlive all created module engines.
class ModuleApiAdapter {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  ModuleApiAdapter(EngineDescriptor,std::shared_ptr<IEngineFactory>);
  ~ModuleApiAdapter();
  const AnaieroModuleApiV1* api()const noexcept;
};
// Process module entry point. Uses bounded chunked stdin/stdout framing and
// exactly one engine per process. Diagnostics belong on stderr.
int run_module_stdio(EngineDescriptor,std::shared_ptr<IEngineFactory>);
}

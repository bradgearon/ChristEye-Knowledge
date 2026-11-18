#pragma once
#include "anaiero/sdk/engine.hpp"
#include <memory>

namespace anaiero::sdk {
class EngineModuleCatalog {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  explicit EngineModuleCatalog(std::size_t capacity=32);
  ~EngineModuleCatalog();
  EngineModuleCatalog(const EngineModuleCatalog&)=delete;
  EngineModuleCatalog& operator=(const EngineModuleCatalog&)=delete;
  Result add(EngineDescriptor,std::shared_ptr<IEngineFactory>);
  Result freeze();
  bool frozen() const noexcept;
  std::size_t size() const noexcept;
  const EngineDescriptor* find(std::string_view type_id) const noexcept;
  std::vector<EngineDescriptor> describe() const;
  Result create(const EngineIdentity&,const Envelope&,const EngineState*,std::unique_ptr<IEngine>&);
};
}

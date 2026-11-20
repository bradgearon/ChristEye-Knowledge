#include "counter_engine.hpp"
#include <anaiero/sdk/module_host.hpp>
#if defined(ANAIERO_MODULE_BUILD)
extern "C" ANAIERO_MODULE_EXPORT const AnaieroModuleApiV1* ANAIERO_MODULE_CALL anaiero_module_v1(void) {
  static anaiero::sdk::ModuleApiAdapter adapter(independent::descriptor(),std::make_shared<independent::Factory>());
  return adapter.api();
}
#else
int main(){return anaiero::sdk::run_module_stdio(independent::descriptor(),std::make_shared<independent::Factory>());}
#endif

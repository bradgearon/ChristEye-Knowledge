#include <anaiero/sdk/module_abi.h>
extern "C" ANAIERO_MODULE_EXPORT const AnaieroModuleApiV1* ANAIERO_MODULE_CALL anaiero_module_v1(void) {
  static AnaieroModuleApiV1 api{};api.struct_size=sizeof(api);api.abi_version=99;return &api;
}

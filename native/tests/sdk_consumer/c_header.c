#include <anaiero/sdk/module_abi.h>
int anaiero_c_abi_header_compiles(void) {
  AnaieroBytesV1 bytes={0,0};
  return bytes.size==0&&ANAIERO_MODULE_ABI_VERSION==1;
}

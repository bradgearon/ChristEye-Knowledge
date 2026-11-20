#ifndef ANAIERO_SDK_MODULE_ABI_H
#define ANAIERO_SDK_MODULE_ABI_H
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32)
#define ANAIERO_MODULE_CALL __cdecl
#if defined(ANAIERO_MODULE_BUILD)
#define ANAIERO_MODULE_EXPORT __declspec(dllexport)
#else
#define ANAIERO_MODULE_EXPORT
#endif
#else
#define ANAIERO_MODULE_CALL
#define ANAIERO_MODULE_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define ANAIERO_MODULE_ABI_VERSION 1u
typedef struct AnaieroBytesV1 {const uint8_t* data;uint64_t size;} AnaieroBytesV1;
typedef struct AnaieroBufferV1 {uint8_t* data;uint64_t capacity;uint64_t written;} AnaieroBufferV1;
/* Canonical ASK1 wire records only; no STL, exceptions, allocation ownership or
 * compiler vtables cross this boundary. Callbacks return SDK Code integers.
 * The host owns output buffers. The module owns handles until destroy().
 * All callbacks execute serially. Native modules are explicitly trusted: this
 * ABI does not sandbox callbacks or promise preemption of arbitrary code. */
typedef struct AnaieroModuleApiV1 {
  uint32_t struct_size,abi_version;
  void* context;
  int32_t (ANAIERO_MODULE_CALL *describe)(void*,AnaieroBufferV1*);
  int32_t (ANAIERO_MODULE_CALL *create)(void*,AnaieroBytesV1,void**);
  void (ANAIERO_MODULE_CALL *destroy)(void*,void*);
  int32_t (ANAIERO_MODULE_CALL *state)(void*,void*,AnaieroBufferV1*);
  int32_t (ANAIERO_MODULE_CALL *prepare)(void*,void*,AnaieroBytesV1,AnaieroBufferV1*);
  int32_t (ANAIERO_MODULE_CALL *install)(void*,void*,AnaieroBytesV1);
  int32_t (ANAIERO_MODULE_CALL *restore)(void*,void*,AnaieroBytesV1);
  int32_t (ANAIERO_MODULE_CALL *fork)(void*,void*,void**);
  int32_t (ANAIERO_MODULE_CALL *change)(void*,void*,AnaieroBytesV1,AnaieroBufferV1*);
  void (ANAIERO_MODULE_CALL *cancel)(void*,void*);
} AnaieroModuleApiV1;
typedef const AnaieroModuleApiV1* (ANAIERO_MODULE_CALL *AnaieroModuleEntryV1)(void);
ANAIERO_MODULE_EXPORT const AnaieroModuleApiV1* ANAIERO_MODULE_CALL anaiero_module_v1(void);
#ifdef __cplusplus
}
#endif
#endif

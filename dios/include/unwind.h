//===------------------------------- unwind.h -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//
// C++ ABI Level 1 ABI documented at:
//   http://mentorembedded.github.io/cxx-abi/abi-eh.html
//
//===----------------------------------------------------------------------===//

#ifndef __UNWIND_H__
#define __UNWIND_H__

#define _LIBUNWIND_ARM_EHABI 0

#include <stdint.h>
#include <stddef.h>

typedef enum {
  _URC_NO_REASON = 0,
  _URC_OK = 0,
  _URC_FOREIGN_EXCEPTION_CAUGHT = 1,
  _URC_FATAL_PHASE2_ERROR = 2,
  _URC_FATAL_PHASE1_ERROR = 3,
  _URC_NORMAL_STOP = 4,
  _URC_END_OF_STACK = 5,
  _URC_HANDLER_FOUND = 6,
  _URC_INSTALL_CONTEXT = 7,
  _URC_CONTINUE_UNWIND = 8,
#if _LIBUNWIND_ARM_EHABI
  _URC_FAILURE = 9
#endif
} _Unwind_Reason_Code;

typedef enum {
  _UA_SEARCH_PHASE = 1,
  _UA_CLEANUP_PHASE = 2,
  _UA_HANDLER_FRAME = 4,
  _UA_FORCE_UNWIND = 8,
  _UA_END_OF_STACK = 16 // gcc extension to C++ ABI
} _Unwind_Action;

typedef struct _Unwind_Context _Unwind_Context;   // opaque

struct _Unwind_Context;   // opaque
struct _Unwind_Exception; // forward declaration
typedef struct _Unwind_Exception _Unwind_Exception;

struct _Unwind_Exception {
  uint64_t exception_class;
  void (*exception_cleanup)(_Unwind_Reason_Code reason,
                            _Unwind_Exception *exc);
  uintptr_t private_1; // non-zero means forced unwind
  uintptr_t private_2; // holds sp that phase1 found for phase2 to use
} __attribute__((aligned(16)));

typedef _Unwind_Reason_Code (*_Unwind_Stop_Fn)
    (int version,
     _Unwind_Action actions,
     uint64_t exceptionClass,
     _Unwind_Exception* exceptionObject,
     struct _Unwind_Context* context,
     void* stop_parameter );

typedef _Unwind_Reason_Code (*__personality_routine)
      (int version,
       _Unwind_Action actions,
       uint64_t exceptionClass,
       _Unwind_Exception* exceptionObject,
       struct _Unwind_Context* context);

#ifdef __cplusplus
extern "C" {
#endif

//
// The following are the base functions documented by the C++ ABI
//
#ifdef __USING_SJLJ_EXCEPTIONS__
extern _Unwind_Reason_Code
    _Unwind_SjLj_RaiseException(_Unwind_Exception *exception_object);
extern void _Unwind_SjLj_Resume(_Unwind_Exception *exception_object);
#else
extern _Unwind_Reason_Code
    _Unwind_RaiseException(_Unwind_Exception *exception_object);
extern void _Unwind_Resume(_Unwind_Exception *exception_object);
#endif
extern void _Unwind_DeleteException(_Unwind_Exception *exception_object);

#if _LIBUNWIND_ARM_EHABI
typedef enum {
  _UVRSC_CORE = 0, /* integer register */
  _UVRSC_VFP = 1, /* vfp */
  _UVRSC_WMMXD = 3, /* Intel WMMX data register */
  _UVRSC_WMMXC = 4 /* Intel WMMX control register */
} _Unwind_VRS_RegClass;

typedef enum {
  _UVRSD_UINT32 = 0,
  _UVRSD_VFPX = 1,
  _UVRSD_UINT64 = 3,
  _UVRSD_FLOAT = 4,
  _UVRSD_DOUBLE = 5
} _Unwind_VRS_DataRepresentation;

typedef enum {
  _UVRSR_OK = 0,
  _UVRSR_NOT_IMPLEMENTED = 1,
  _UVRSR_FAILED = 2
} _Unwind_VRS_Result;

extern void _Unwind_Complete(_Unwind_Exception* exception_object);

extern _Unwind_VRS_Result
_Unwind_VRS_Get(_Unwind_Context *context, _Unwind_VRS_RegClass regclass,
                uint32_t regno, _Unwind_VRS_DataRepresentation representation,
                void *valuep);

extern _Unwind_VRS_Result
_Unwind_VRS_Set(_Unwind_Context *context, _Unwind_VRS_RegClass regclass,
                uint32_t regno, _Unwind_VRS_DataRepresentation representation,
                void *valuep);

extern _Unwind_VRS_Result
_Unwind_VRS_Pop(_Unwind_Context *context, _Unwind_VRS_RegClass regclass,
                uint32_t discriminator,
                _Unwind_VRS_DataRepresentation representation);
#endif

#if !_LIBUNWIND_ARM_EHABI

extern uintptr_t _Unwind_GetGR(struct _Unwind_Context *context, int index);
extern void _Unwind_SetGR(struct _Unwind_Context *context, int index,
                          uintptr_t new_value);
extern uintptr_t _Unwind_GetIP(struct _Unwind_Context *context);
extern void _Unwind_SetIP(struct _Unwind_Context *, uintptr_t new_value);

#else  // _LIBUNWIND_ARM_EHABI

#if defined(_LIBUNWIND_UNWIND_LEVEL1_EXTERNAL_LINKAGE)
#define _LIBUNWIND_EXPORT_UNWIND_LEVEL1 extern
#else
#define _LIBUNWIND_EXPORT_UNWIND_LEVEL1 static __inline__
#endif

// These are de facto helper functions for ARM, which delegate the function
// calls to _Unwind_VRS_Get/Set().  These are not a part of ARM EHABI
// specification, thus these function MUST be inlined.  Please don't replace
// these with the "extern" function declaration; otherwise, the program
// including this <unwind.h> header won't be ABI compatible and will result in
// link error when we are linking the program with libgcc.

_LIBUNWIND_EXPORT_UNWIND_LEVEL1
uintptr_t _Unwind_GetGR(struct _Unwind_Context *context, int index) {
  uintptr_t value = 0;
  _Unwind_VRS_Get(context, _UVRSC_CORE, (uint32_t)index, _UVRSD_UINT32, &value);
  return value;
}

_LIBUNWIND_EXPORT_UNWIND_LEVEL1
void _Unwind_SetGR(struct _Unwind_Context *context, int index,
                   uintptr_t value) {
  _Unwind_VRS_Set(context, _UVRSC_CORE, (uint32_t)index, _UVRSD_UINT32, &value);
}

_LIBUNWIND_EXPORT_UNWIND_LEVEL1
uintptr_t _Unwind_GetIP(struct _Unwind_Context *context) {
  // remove the thumb-bit before returning
  return _Unwind_GetGR(context, 15) & (~(uintptr_t)0x1);
}

_LIBUNWIND_EXPORT_UNWIND_LEVEL1
void _Unwind_SetIP(struct _Unwind_Context *context, uintptr_t value) {
  uintptr_t thumb_bit = _Unwind_GetGR(context, 15) & ((uintptr_t)0x1);
  _Unwind_SetGR(context, 15, value | thumb_bit);
}
#endif  // _LIBUNWIND_ARM_EHABI

extern uintptr_t _Unwind_GetRegionStart(struct _Unwind_Context *context);
extern uintptr_t
    _Unwind_GetLanguageSpecificData(struct _Unwind_Context *context);
#ifdef __USING_SJLJ_EXCEPTIONS__
extern _Unwind_Reason_Code
    _Unwind_SjLj_ForcedUnwind(_Unwind_Exception *exception_object,
                              _Unwind_Stop_Fn stop, void *stop_parameter);
#else
extern _Unwind_Reason_Code
    _Unwind_ForcedUnwind(_Unwind_Exception *exception_object,
                         _Unwind_Stop_Fn stop, void *stop_parameter);
#endif

#ifdef __USING_SJLJ_EXCEPTIONS__
typedef struct _Unwind_FunctionContext *_Unwind_FunctionContext_t;
extern void _Unwind_SjLj_Register(_Unwind_FunctionContext_t fc);
extern void _Unwind_SjLj_Unregister(_Unwind_FunctionContext_t fc);
#endif

//
// The following are semi-suppoted extensions to the C++ ABI
//

//
//  called by __cxa_rethrow().
//
#ifdef __USING_SJLJ_EXCEPTIONS__
extern _Unwind_Reason_Code
    _Unwind_SjLj_Resume_or_Rethrow(_Unwind_Exception *exception_object);
#else
extern _Unwind_Reason_Code
    _Unwind_Resume_or_Rethrow(_Unwind_Exception *exception_object);
#endif

#ifdef __cplusplus
}
#endif

#endif // __UNWIND_H__

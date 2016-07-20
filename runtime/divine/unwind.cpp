// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>

// based on documentation at https://mentorembedded.github.io/cxx-abi/abi-eh.html

#include <unwind.h>
#include <divine.h>
#include <divine/metadata.h>

struct _Unwind_Context {
    uintptr_t pc() const { return uintptr_t( _frame->pc ); }
    void pc( uintptr_t v ) { _frame->pc = reinterpret_cast< void (*)() >( v ); }
    const _MD_Function &meta() {
        if ( !_meta )
            _meta = __md_get_pc_meta( uintptr_t( _frame->pc ) );
        return *_meta;
    }

    _VM_Frame *_frame;
    const _MD_Function *_meta;
};

static const int unwindVersion = 1;

//  This function sets the 64-bit value of the given register, identified by
//  its index as for _Unwind_GetGR. The NAT bit of the given register is reset.
//
//  The behaviour is guaranteed only if the function is called during phase 2
//  of unwinding, and applied to an unwind context representing a handler
//  frame, for which the personality routine will return _URC_INSTALL_CONTEXT.
//  In that case, only registers GR15, GR16, GR17, GR18 should be used. These
//  scratch registers are reserved for passing arguments between the
//  personality routine and the landing pads.
void _Unwind_SetGR( _Unwind_Context *ctx, int index, uintptr_t value ) {
    if ( index < 0 )
        __vm_fault( _VM_F_Assert, "Register index cannot be negative" );
    if ( index > 1 )
        __vm_fault( _VM_F_Assert, "Unsupported register" );
    // reg 0 - exception object
    // reg 1 - type ID

    // TODO:
    // 1. find landingpad instruction
    // 2. set its return value
    __vm_fault( _VM_F_NotImplemented );
}

//  This function sets the value of the instruction pointer (IP) for the
//  routine identified by the unwind context.
//
//  The behaviour is guaranteed only when this function is called for an unwind
//  context representing a handler frame, for which the personality routine
//  will return _URC_INSTALL_CONTEXT. In this case, control will be transferred
//  to the given address, which should be the address of a landing pad.
void _Unwind_SetIP( _Unwind_Context *ctx, uintptr_t value ) {
    // TODO: validate it is a landingpad of the original function
    ctx->pc( value );
}

//  This function returns the 64-bit value of the given general register. The
//  register is identified by its index: 0 to 31 are for the fixed registers,
//  and 32 to 127 are for the stacked registers.
//
//  During the two phases of unwinding, only GR1 has a guaranteed value, which
//  is the Global Pointer (GP) of the frame referenced by the unwind context.
//  If the register has its NAT bit set, the behaviour is unspecified.
uintptr_t _Unwind_GetGR( _Unwind_Context *ctx, int index ) {
    // from doc: only GR1 has a guaranteed value, which the Global Pointer (GP)
    // of the frame referenced by the unwind context
    __vm_fault( _VM_F_NotImplemented );
}

//  This function returns the 64-bit value of the instruction pointer (IP).
//
//  During unwinding, the value is guaranteed to be the address of the bundle
//  immediately following the call site in the function identified by the
//  unwind context. This value may be outside of the procedure fragment for a
//  function call that is known to not return (such as _Unwind_Resume).
uintptr_t _Unwind_GetIP( _Unwind_Context *ctx ) {
    // should point immediatelly AFTER the call site
    return ctx->pc() + 1;
}

//  Raise an exception, passing along the given exception object, which should
//  have its exception_class and exception_cleanup fields set. The exception
//  object has been allocated by the language-specific runtime, and has a
//  language-specific format, except that it must contain an _Unwind_Exception
//  struct (see Exception Header above). _Unwind_RaiseException does not
//  return, unless an error condition is found (such as no handler for the
//  exception, bad stack format, etc.). In such a case, an _Unwind_Reason_Code
//  value is returned. Possibilities are:
//
//      _URC_END_OF_STACK: The unwinder encountered the end of the stack during
//      phase 1, without finding a handler. The unwind runtime will not have
//      modified the stack. The C++ runtime will normally call
//      uncaught_exception() in this case.
//
//      _URC_FATAL_PHASE1_ERROR: The unwinder encountered an unexpected error
//      during phase 1, e.g. stack corruption. The unwind runtime will not have
//      modified the stack. The C++ runtime will normally call terminate() in
//      this case.
//
//  If the unwinder encounters an unexpected error during phase 2, it should
//  return _URC_FATAL_PHASE2_ERROR to its caller. In C++, this will usually be
//  __cxa_throw, which will call terminate().
_Unwind_Reason_Code _Unwind_RaiseException( _Unwind_Exception *exception ) {
    __vm_fault( _VM_F_NotImplemented );
}

//  Resume propagation of an existing exception e.g. after executing cleanup
//  code in a partially unwound stack. A call to this routine is inserted at
//  the end of a landing pad that performed cleanup, but did not resume normal
//  execution. It causes unwinding to proceed further.
void _Unwind_Resume( _Unwind_Exception *exception ) {
    __vm_fault( _VM_F_NotImplemented );
}

// Deletes the given exception object. If a given runtime resumes normal
// execution after catching a foreign exception, it will not know how to delete
// that exception. Such an exception will be deleted by calling
// _Unwind_DeleteException. This is a convenience function that calls the
// function pointed to by the exception_cleanup field of the exception header.
void _Unwind_DeleteException( _Unwind_Exception *exception ) {
    if ( exception->exception_cleanup != nullptr )
        (*exception->exception_cleanup)( _URC_FOREIGN_EXCEPTION_CAUGHT,
                                                exception );
}

// This routine returns the address of the language-specific data area for the
// current stack frame.
uintptr_t _Unwind_GetLanguageSpecificData( _Unwind_Context *ctx ) {
    __vm_fault( _VM_F_NotImplemented );
}

//  This routine returns the address of the beginning of the procedure or code
//  fragment described by the current unwind descriptor block.
//
//  This information is required to access any data stored relative to the
//  beginning of the procedure fragment. For instance, a call site table might
//  be stored relative to the beginning of the procedure fragment that contains
//  the calls. During unwinding, the function returns the start of the
//  procedure fragment containing the call site in the current stack frame.
uintptr_t _Unwind_GetRegionStart( _Unwind_Context *ctx ) {
    return uintptr_t( ctx->meta().entry_point );
}



// -*- C++ -*- (c) 2016-2017 Vladimír Štill <xstill@fi.muni.cz>

// based on documentation at https://mentorembedded.github.io/cxx-abi/abi-eh.html

#include <unwind.h>
#include <sys/divm.h>
#include <sys/start.h>
#include <sys/interrupt.h>
#include <sys/fault.h>
#include <sys/metadata.h>
#include <sys/bitcode.h>
#include <dios.h>
#include <limits.h>
#include <string.h>

struct _Unwind_Context
{
    _Unwind_Context() = default;
    _Unwind_Context( _VM_Frame *f ) : frame( f ) { }
    _Unwind_Context( _Unwind_Exception *exc ) :
        frame( reinterpret_cast< _VM_Frame * >( exc->private_1 ) ), jumpPC( exc->private_2 )
    { }

    void fill( _Unwind_Exception *exc ) {
        exc->private_1 = uintptr_t( frame );
        exc->private_2 = jumpPC;
    }

    _VM_CodePointer pc() const { return frame->pc; }

    void next() {
        frame = frame->parent;
        jumpPC = 0;
    }

    bool valid() const { return frame; }
    explicit operator bool() const { return valid(); }

    _VM_Frame *frame = nullptr;
    uintptr_t jumpPC = 0; // PC to jump to when unwinding (landing block)
};

static const int unwindVersion = 1;

static _VM_CodePointer getLandingPad( _VM_CodePointer pc ) {
    auto lb = __dios_get_landing_block( pc );
    return __dios_find_instruction_in_block( lb, 1, OpCode::LandingPad );
}

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
    __dios::InterruptMask _;
    __dios_assert_v( index >= 0, "Register index cannot be negative" );
    __dios_assert_v( index <= 1, "Unsupported register" );
    // reg 0 - exception object
    // reg 1 - type ID
    // landingpad { i8*, i32 }

    auto lp = getLandingPad( ctx->pc() );

    if ( index == 0 )
        __dios_set_register( ctx->frame, lp, 0, value, sizeof( uintptr_t ) );
    else if ( index == 1 ) {
        __dios_assert_v( intptr_t( value ) >= intptr_t( INT_MIN ), "Overflow in exception type index" );
        __dios_assert_v( intptr_t( value ) <= intptr_t( INT_MAX ), "Underflow in exception type index" );
        __dios_set_register( ctx->frame, lp, sizeof( uintptr_t ), int( value ), sizeof( int ) );
    }
}

//  This function sets the value of the instruction pointer (IP) for the
//  routine identified by the unwind context.
//
//  The behaviour is guaranteed only when this function is called for an unwind
//  context representing a handler frame, for which the personality routine
//  will return _URC_INSTALL_CONTEXT. In this case, control will be transferred
//  to the given address, which should be the address of a landing pad.
void _Unwind_SetIP( _Unwind_Context *ctx, uintptr_t value ) {
    ctx->jumpPC = value;
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

    __dios::InterruptMask _;
    __dios_assert_v( index >= 0, "Register index cannot be negative" );
    __dios_assert_v( index <= 1, "Unsupported register" );
    // reg 0 - exception object
    // reg 1 - type ID
    // landingpad { i8*, i32 }

    auto lp = getLandingPad( ctx->pc() );

    if ( index >= 0 && index <= 1 )
        return __dios_get_register( ctx->frame, lp, index == 0 ? 0 : sizeof( uintptr_t ),
                                    index == 0 ? sizeof( uintptr_t ) : sizeof( int ) );
    __dios_fault( _VM_F_NotImplemented, "invalid register" );
    __builtin_trap();
}

//  This function returns the 64-bit value of the instruction pointer (IP).
//
//  During unwinding, the value is guaranteed to be the address of the bundle
//  immediately following the call site in the function identified by the
//  unwind context. This value may be outside of the procedure fragment for a
//  function call that is known to not return (such as _Unwind_Resume).
uintptr_t _Unwind_GetIP( _Unwind_Context *ctx ) {
    __dios::InterruptMask _;
    // should point immediatelly AFTER the call site
    return 1 + uintptr_t( __dios_find_instruction_in_block( ctx->pc(), -1, OpCode::OpBB ) );
}

static inline bool shouldCallPersonality( _DiOS_FunAttrs &attrs, _Unwind_Context &ctx ) {
    return attrs.personality
            // we must not call personality if there is not active invoke in
            // the targert, it could overflow the EH table
            && __dios_get_opcode( ctx.pc() ) == OpCode::Invoke;
}

static const uint64_t cppExceptionClass          = 0x434C4E47432B2B00; // CLNGC++\0
static const uint64_t cppDependentExceptionClass = 0x434C4E47432B2B01; // CLNGC++\1

// internal, used in _Unwind_RaiseException and _Unwind_Resume for the actual
// stack unwinding
static _Unwind_Reason_Code _Unwind_Phase2( _VM_Frame *topFrame, _Unwind_Context &foundCtx,
                                   _Unwind_Exception *exception, __dios::InterruptMask &mask )
    __attribute__((__annotate__("lart.interrupt.skipcfl")))
{
    for ( _Unwind_Context ctx( topFrame ); ctx; ctx.next() )
    {
        auto attrs = __dios_get_func_exception_attrs( ctx.pc() );
        if ( !shouldCallPersonality( attrs, ctx ) )
            continue;
        auto pers = reinterpret_cast< __personality_routine >( attrs.personality );
        int flags = _UA_CLEANUP_PHASE;
        if ( ctx.frame == foundCtx.frame )
            flags |= _UA_HANDLER_FRAME;
        auto r = mask.without( [&] {
            return pers( unwindVersion, _Unwind_Action( flags ), exception->exception_class, exception, &ctx );
        } );
        if ( (flags & _UA_HANDLER_FRAME) && r != _URC_INSTALL_CONTEXT ) {
            __dios_trace( 0, "Unwinder Fatal Error: Frame which indicated handler in phase 1 refused in phase 2" );
            return _URC_FATAL_PHASE2_ERROR;
        }
        if ( r == _URC_INSTALL_CONTEXT ) {
            // save mask state before it is removed from the stack
            auto maskState = mask._origState();
            // kill the part of stack which will be jumped over
            __dios_unwind( nullptr, nullptr, ctx.frame );
            __dios_assert( ctx.jumpPC != 0 );
            // unwinder has to put the mask to the same state as it was before throw/resume
            __dios_jump( ctx.frame, reinterpret_cast< void (*)() >( ctx.jumpPC ), maskState );
        }
    }
    return _URC_END_OF_STACK;
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
_Unwind_Reason_Code _Unwind_RaiseException( _Unwind_Exception *exception )
    __attribute__((__annotate__("lart.interrupt.skipcfl")))
{
    __dios::InterruptMask mask;

    auto *topFrame = __dios_this_frame()->parent;
    _Unwind_Context foundCtx;

    for ( _Unwind_Context ctx( topFrame ); ctx; ctx.next() )
    {
        auto attrs = __dios_get_func_exception_attrs( ctx.pc() );
        if ( shouldCallPersonality( attrs, ctx ) )
        {
            auto pers = reinterpret_cast< __personality_routine >( attrs.personality );
            // personality in not part of the unwinder and therefore should be
            // allowed to interleave with other threads
            auto r = mask.without( [&] {
                return pers( unwindVersion, _UA_SEARCH_PHASE, exception->exception_class, exception, &ctx );
            } );
            if ( r == _URC_HANDLER_FOUND ) {
                foundCtx = ctx;
                break;
            }
        }

        if ( attrs.is_nounwind &&
             __md_get_pc_meta( ctx.pc() )->entry_point != reinterpret_cast< void (*)() >( &_start ) )
        {
            /* TODO demangle the name? */
            __dios_trace_f( "unexpected nounwind function: %s", __md_get_pc_meta( ctx.pc() )->name );
            __dios_fault( _VM_F_Control, "Exception thrown out of nounwind function" );
        }
    }

    if ( !foundCtx )
        return _URC_END_OF_STACK;

    foundCtx.fill( exception );

    _Unwind_Reason_Code r = _Unwind_Phase2( topFrame, foundCtx, exception, mask );

    // Phase2 des not return unless there was an error
    __dios_trace( 0, "Unwinder Fatal Error: handler not found in phase 2" );
    return r;
}

//  Resume propagation of an existing exception e.g. after executing cleanup
//  code in a partially unwound stack. A call to this routine is inserted at
//  the end of a landing pad that performed cleanup, but did not resume normal
//  execution. It causes unwinding to proceed further.
void _Unwind_Resume( _Unwind_Exception *exception ) {
    __dios::InterruptMask mask;
    auto topFrame = __dios_this_frame()->parent;
    _Unwind_Context foundCtx( exception );
    _Unwind_Phase2( topFrame, foundCtx, exception, mask );
    __dios_fault( _VM_F_Control,
                  "Unwinder Fatal Error: resume did not found any exception handler" );
    __builtin_trap();
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
    __dios::InterruptMask _;
    return uintptr_t( __dios_get_func_exception_attrs( ctx->pc() ).lsda );
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
    return uintptr_t( __dios_get_function_entry( ctx->pc() ) );
}

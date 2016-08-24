// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>


#include <dios/fault.h>
#include <dios/trace.h>

namespace __dios {

void fault( enum _VM_Fault what, _VM_Frame *cont_frame, void (*cont_pc)() ) noexcept {
    auto mask = __vm_mask( 1 );
    InTrace _; // avoid dumping what we do

    __dios_trace_t( "VM Fault" );
    switch ( static_cast< int >( what ) ) {
    case _VM_F_NoFault:
        __dios_trace_t( "FAULT: No fault" );
        break;
    case _VM_F_Assert:
        __dios_trace_t( "FAULT: Assert" );
        break;
    case _VM_F_Arithmetic:
        __dios_trace_t( "FAULT: Arithmetic" );
        break;
    case _VM_F_Memory:
        __dios_trace_t( "FAULT: Memory" );
        break;
    case _VM_F_Control:
        __dios_trace_t( "FAULT: Control" );
        break;
    case _VM_F_Locking:
        __dios_trace_t( "FAULT: Locking" );
        break;
    case _VM_F_Hypercall:
        __dios_trace_t( "FAULT: Hypercall" );
        break;
    case _VM_F_NotImplemented:
        __dios_trace_t( "FAULT: Not Implemented" );
        break;
    case _DiOS_F_Threading:
        __dios_trace_t( "FAULT: Threading error occured" );
        break;
    case _DiOS_F_Assert:
        __dios_trace_t( "FAULT: DiOS assert" );
        break;
    case _DiOS_F_MissingFunction:
        __dios_trace_t( "FAULT: Missing function" );
        break;
    case _DiOS_F_MainReturnValue:
        __dios_trace_t( "FAULT: main exited with non-zero code" );
        break;
    default:
        __dios_trace_t( "Unknown fault ");
    }
    __dios_trace_t( "Backtrace:" );
    int i = 0;
    for ( auto *f = __vm_query_frame()->parent; f != nullptr; f = f->parent )
        traceInternal( 0, "%d: %s", ++i, __md_get_pc_meta( uintptr_t( f->pc ) )->name );

    __vm_jump( cont_frame, cont_pc, !mask );
    __builtin_unreachable();
}

} // namespace __dios

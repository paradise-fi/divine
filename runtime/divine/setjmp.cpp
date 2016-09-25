// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>

#include <setjmp.h>
#include <divine/opcodes.h>
#include <dios.h>
#include <cassert>

int setjmp( jmp_buf env ) {
    _VM_Frame *frame = static_cast< _VM_Frame * >( __vm_control( _VM_CA_Get, _VM_CR_Frame ) )->parent;
    env->__jumpFrame = frame;
    uintptr_t pc = uintptr_t( frame->pc );
    auto *meta = __md_get_pc_meta( pc );
    auto op = meta->inst_table[ pc - uintptr_t( meta->entry_point ) ].opcode;
    assert( op == OpCode::Call );
    env->__jumpPC = pc;
    env->__jumpFunctionMeta = meta;
    return 0;
}

void longjmp( jmp_buf env, int val ) {
    auto retreg = __md_get_register_info( env->__jumpFrame, env->__jumpPC, env->__jumpFunctionMeta );
    assert( retreg.width == sizeof( int ) );
    if ( val == 0 )
        val = 1;
    *reinterpret_cast< int * >( retreg.start ) = val;
    __dios_unwind( env->__jumpFrame, reinterpret_cast< void (*)() >( env->__jumpPC + 1 ) );
}

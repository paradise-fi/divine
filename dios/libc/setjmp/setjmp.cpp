// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>

#include <setjmp.h>
#include <sys/bitcode.h>
#include <dios.h>
#include <assert.h>

int setjmp( jmp_buf env )
{
    _VM_Frame *frame = __dios_this_frame()->parent;
    env->__jumpFrame = frame;
    const _VM_CodePointer pc = frame->pc;
    auto *meta = __md_get_pc_meta( frame->pc );
    auto op = meta->inst_table[ uintptr_t( pc ) - uintptr_t( meta->entry_point ) ].opcode;
    assert( op == OpCode::Call );
    env->__jumpPC = pc;
    env->__jumpFunctionMeta = meta;
    return 0;
}

/* writting to frame must bypass weakmem */
__weakmem_direct __attribute__((__noinline__)) static void _longjmp_setretval( int *addr, int val )
{
    *addr = val;
}

void longjmp( jmp_buf env, int val )
{
    auto retreg = __md_get_register_info( env->__jumpFrame, env->__jumpPC, env->__jumpFunctionMeta );
    assert( retreg.width == sizeof( int ) );
    if ( val == 0 )
        val = 1;
    _longjmp_setretval( reinterpret_cast< int * >( retreg.start ), val );
    __dios_stack_free( __dios_parent_frame(), env->__jumpFrame );
    __dios_jump( env->__jumpFrame, _VM_CodePointer( uintptr_t( env->__jumpPC ) ), -1 );
}

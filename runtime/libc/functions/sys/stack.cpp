#include <sys/divm.h>
#include <divine/metadata.h>
#include <sys/bitcode.h>
#include <string.h>

_Noreturn void __dios_jump( _VM_Frame *to, _VM_CodePointer pc, int restoreMaskTo ) noexcept
{
    bool m = reinterpret_cast< uintptr_t >(
        __vm_control( _VM_CA_Get, _VM_CR_Flags,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask ) ) & _VM_CF_Mask;
    if ( restoreMaskTo != -1 )
        m = restoreMaskTo;

    if ( m )
        __vm_control( _VM_CA_Set, _VM_CR_Frame, to,
                      _VM_CA_Set, _VM_CR_PC, pc );
    else
        __vm_control( _VM_CA_Set, _VM_CR_Frame, to,
                      _VM_CA_Set, _VM_CR_PC, pc,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, 0ull );
    __builtin_unreachable();
}

void __dios_unwind( _VM_Frame *stack, _VM_Frame *from, _VM_Frame *to ) noexcept
{
    bool m = reinterpret_cast< uintptr_t >(
        __vm_control( _VM_CA_Get, _VM_CR_Flags,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask ) ) & _VM_CF_Mask;
    if ( !stack ) {
        stack = static_cast< _VM_Frame * >( __vm_control( _VM_CA_Get, _VM_CR_Frame ) );
        // we must leave our caller intact so we can return to it
        if ( !from )
            from = stack->parent->parent;
    } else if ( !from )
        from = stack;

    _VM_Frame *fakeParentUpdateLocation;
    _VM_Frame **parentUpdateLocation = &fakeParentUpdateLocation;
    if ( stack != from ) {
        auto *f = stack;
        for ( ; f && f->parent != from; f = f->parent ) { }
        __dios_assert_v( f, "__dios_unwind reached end of the stack and did not find 'from' frame" );
        parentUpdateLocation = &f->parent;
    }

    // clean the frames, drop their allocas, jump
    // note: it is not necessary to clean the frames, it is only to prevent
    // accidental use of their variables, therefore it is also OK to not clean
    // current frame (heap will garbage-colect it)
    for ( auto *f = from; f != to; ) {
        auto *meta = __md_get_pc_meta( f->pc );
        auto *inst = meta->inst_table;
        for ( int i = 0; i < meta->inst_table_size; ++i, ++inst )
        {
            if ( inst->opcode == OpCode::Alloca )
            {
                _VM_CodePointer pc = _VM_CodePointer( uintptr_t( meta->entry_point ) + i );
                void *regaddr = __md_get_register_info( f, pc, meta ).start;
                void *regval = *static_cast< void ** >( regaddr );
                __vm_obj_free( regval );
            }
        }
        auto *old = f;
        f = f->parent;
        *parentUpdateLocation = f;
        __dios_assert_v( f || !to, "__dios_unwind did not find the target frame" );
        __vm_obj_free( old );
    }

    if ( !m )
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, 0ull );
}

int __dios_set_register( _VM_Frame *frame, _VM_CodePointer pc, unsigned offset, char *data, unsigned lenght )
{
    auto *meta = __md_get_pc_meta( pc );
    auto info = __md_get_register_info( frame, pc, meta );
    if ( unsigned( info.width ) < offset + lenght )
        return 0;

    memcpy( reinterpret_cast< char * >( info.start ) + offset, data, lenght );
    return 1;
}

int __dios_get_register( _VM_Frame *frame, _VM_CodePointer pc, unsigned offset, char *data, unsigned lenght )
{
    auto *meta = __md_get_pc_meta( pc );
    auto info = __md_get_register_info( frame, pc, meta );
    if ( unsigned( info.width ) < offset + lenght )
        return 0;

    memcpy( data, reinterpret_cast< char * >( info.start ) + offset, lenght );
    return 1;
}

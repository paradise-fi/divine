#include <sys/divm.h>
#include <sys/metadata.h>
#include <sys/bitcode.h>
#include <string.h>

/* NOTE: any write directly to a stack frame should bypass weakmem as registers
 * and parent pointers are read by the VM */
__invisible __weakmem_direct void __dios_unwind( _VM_Frame *stack, _VM_Frame *from, _VM_Frame *to ) noexcept
{
    bool noret = false;
    if ( !stack )
    {
        stack = static_cast< _VM_Frame * >( __vm_ctl_get( _VM_CR_Frame ) );
        // we must leave our caller intact so we can return to it
        if ( from == stack->parent )
            noret = true; /* we cannot return */
        if ( !from )
            from = stack->parent->parent;
    } else if ( !from )
        from = stack;

    _VM_Frame *fakeParentUpdateLocation;
    _VM_Frame **parentUpdateLocation = &fakeParentUpdateLocation;
    if ( stack != from )
    {
        auto *f = stack;
        for ( ; f && f->parent != from; f = f->parent ) { }
        __dios_assert_v( f, "__dios_unwind reached end of the stack and did not find 'from' frame" );
        parentUpdateLocation = &f->parent;
    }

    // clean the frames, drop their allocas, jump
    for ( auto *f = from; f != to; )
    {
        auto *meta = __md_get_pc_meta( f->pc );
        auto *inst = meta->inst_table;
        auto base = reinterpret_cast< uint8_t * >( f );
        for ( int i = 0; i < meta->inst_table_size; ++i, ++inst )
            if ( inst->opcode == OpCode::Alloca )
                __dios_safe_free( *reinterpret_cast< void ** >( base + inst->val_offset ) );
        auto *old = f;
        f = f->parent;
        *parentUpdateLocation = f;
        __dios_assert_v( f || !to, "__dios_unwind did not find the target frame" );
        __vm_obj_free( old );
    }

    if ( noret )
        __dios_suicide();
}

/* writing to frame must bypass weakmem to make effects visible to VM */
__weakmem_direct void __dios_set_register( _VM_Frame *frame, _VM_CodePointer pc,
                                           unsigned offset, uint64_t value, unsigned lenght )
{
    assert( lenght <= sizeof( uint64_t ) );
    auto *meta = __md_get_pc_meta( pc );
    auto info = __md_get_register_info( frame, pc, meta );
    assert( unsigned( info.width ) >= offset + lenght );

    memcpy( reinterpret_cast< char * >( info.start ) + offset, &value, lenght );
}

/* no need to bypass weakmem for reading, reading always tries memory */
uint64_t __dios_get_register( _VM_Frame *frame, _VM_CodePointer pc, unsigned offset,
                              unsigned lenght )
{
    assert( lenght <= sizeof( uint64_t ) );
    auto *meta = __md_get_pc_meta( pc );
    auto info = __md_get_register_info( frame, pc, meta );
    assert( unsigned( info.width ) >= offset + lenght );

    uint64_t value = 0;
    memcpy( reinterpret_cast< char * >( value ),
            reinterpret_cast< char * >( info.start ) + offset, lenght );
    return value;
}

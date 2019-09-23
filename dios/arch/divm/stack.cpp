#include <sys/divm.h>
#include <sys/metadata.h>
#include <sys/bitcode.h>
#include <string.h>

__local_invisible __weakmem_direct void __dios_stack_free( _VM_Frame *from, _VM_Frame *to ) noexcept
{
    _VM_Frame *next = nullptr;

    // clean the frames, drop their allocas, jump
    for ( auto *f = from; f && f != to; f = next )
    {
        auto *meta = __md_get_pc_meta( f->pc );
        auto *inst = meta->inst_table;
        auto base = reinterpret_cast< uint8_t * >( f );

        for ( int i = 0; i < meta->inst_table_size; ++i, ++inst )
            if ( inst->opcode == OpCode::Alloca )
                __dios_safe_free( *reinterpret_cast< void ** >( base + inst->val_offset ) );

        next = f->parent;
        __vm_obj_free( f );
    }
}

/* NOTE: any write directly to a stack frame should bypass weakmem as registers
 * and parent pointers are read by the VM */
__local_invisible __weakmem_direct void __dios_stack_cut( _VM_Frame *top, _VM_Frame *bottom ) noexcept
{
    if ( top == bottom )
        return;

    for ( ; top && top->parent != bottom; top = top->parent ) {}
    top->parent = nullptr;
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

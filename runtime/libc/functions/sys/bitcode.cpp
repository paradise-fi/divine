#include <sys/bitcode.h>
#include <divine/metadata.h>

_VM_CodePointer __dios_get_function_entry( _VM_CodePointer pc )
{
    return _VM_CodePointer( uintptr_t( pc ) & ~uintptr_t( _VM_PM_Off ) );
}

_VM_CodePointer __dios_get_landing_block( _VM_CodePointer pc )
{
    auto *meta = __md_get_pc_meta( pc );
    const uintptr_t entry = uintptr_t( __dios_get_function_entry( pc ) );
    return _VM_CodePointer( entry + meta->inst_table[ uintptr_t( pc ) - entry ].subop );
}

_VM_CodePointer __dios_find_instruction_in_block( _VM_CodePointer pc_, int direction, unsigned opcode )
    __attribute__((__annotate__("lart.interrupt.skipcfl")))
{
    auto *meta = __md_get_pc_meta( pc_ );
    const uintptr_t entry = uintptr_t( __dios_get_function_entry( pc_ ) );
    uintptr_t pc = uintptr_t( pc_ ) - entry;
    unsigned op;
    do {
        pc += direction;
    } while ( (op = meta->inst_table[ pc ].opcode) != opcode && op != OpCode::OpBB );
    return (op == OpCode::OpBB && opcode != OpCode::OpBB)
        ? nullptr
        : _VM_CodePointer( entry + pc );
}

_DiOS_FunAttrs __dios_get_func_exception_attrs( _VM_CodePointer pc )
{
    auto *meta = __md_get_pc_meta( pc );
    _DiOS_FunAttrs attrs;
    attrs.personality = meta->ehPersonality;
    attrs.is_nounwind = meta->is_nounwind;
    attrs.lsda = meta->ehLSDA;
    return attrs;
}

unsigned __dios_get_opcode( _VM_CodePointer pc )
{
    return __md_get_pc_meta( pc )->inst_table[ uintptr_t( pc ) - uintptr_t( __dios_get_function_entry( pc ) ) ].opcode;
}

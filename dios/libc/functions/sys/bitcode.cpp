#include <sys/bitcode.h>
#include <sys/metadata.h>

_VM_CodePointer __dios_get_function_entry( _VM_CodePointer pc )
{
    return __md_get_pc_meta( pc )->entry_point;
}

_VM_CodePointer __dios_get_landing_block( _VM_CodePointer pc )
{
    auto *meta = __md_get_pc_meta( pc );
    const uintptr_t base = uintptr_t( pc ) & ~_VM_PM_Off,
                  offset = uintptr_t( pc ) & _VM_PM_Off;
    return _VM_CodePointer( base | meta->inst_table[ offset ].subop );
}

_VM_CodePointer __dios_find_instruction_in_block( _VM_CodePointer pc, int direction, unsigned opcode )
    __attribute__((__annotate__("lart.interrupt.skipcfl")))
{
    auto *meta = __md_get_pc_meta( pc );
    const uintptr_t base = uintptr_t( pc ) & ~_VM_PM_Off;
    uintptr_t offset = uintptr_t( pc ) & _VM_PM_Off;
    unsigned op;
    do {
        offset += direction;
    } while ( (op = meta->inst_table[ offset ].opcode) != opcode && op != OpCode::OpBB );
    return (op == OpCode::OpBB && opcode != OpCode::OpBB)
        ? nullptr
        : _VM_CodePointer( base | offset );
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
    return __md_get_pc_meta( pc )->inst_table[ uintptr_t( pc ) & _VM_PM_Off ].opcode;
}

#include <sys/metadata.h>
#include <string.h>
#include <limits.h>

extern const _MD_Function __md_functions[];
extern const int __md_functions_count;
extern const _MD_Global __md_globals[];
extern const int __md_globals_count;

/*
const _MD_Function *__md_get_function_meta( const char *name ) {
    long from = 0;
    long to = __md_functions_count;
    while ( from < to ) {
        long half = (from + to) / 2;
        int c = std::strcmp( name, __md_functions[ half ].name );
        if ( c < 0 ) // name < half
            to = half;
        else if ( c > 0 ) // name > half
            from = half + 1;
        else
            return __md_functions + half;
    }
    return nullptr;
}
*/

_MD_RegInfo __md_get_register_info( _VM_Frame *frame, _VM_CodePointer pc, const _MD_Function *funMeta )
{
    if ( !frame || !funMeta || !pc )
        return { nullptr, 0 };
    intptr_t offset = uintptr_t( pc ) & _VM_PM_Off;
    if ( offset < 0 || offset > funMeta->inst_table_size )
        return { nullptr, 0 };

    char *base = reinterpret_cast< char * >( frame );
    auto &imeta = funMeta->inst_table[ offset ];
    return { base + imeta.val_offset, imeta.val_width };
}

__invisible const _MD_Global *__md_get_global_meta( const char *name ) noexcept
{
    const auto *b = __md_globals, *e = b + __md_globals_count;
    while ( b < e )
        if ( ::strcmp( b->name, name ) == 0 )
            return b;
        else
            ++ b;
    return nullptr;
}

#include <cstring>
#include <divine/metadata.h>
#include <limits>
#include <algorithm>

extern const _MD_Function __md_functions[];
extern const int __md_functions_count;
extern const _MD_Global __md_globals[];
extern const int __md_globals_count;

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

const _MD_Function *__md_get_pc_meta( uintptr_t _pc ) {
    uintptr_t pc = uintptr_t(_pc );
    uintptr_t dist = std::numeric_limits< intptr_t >::max();
    const _MD_Function *it = __md_functions,
                       *ptr = nullptr;
    for ( int i = 0; i < __md_functions_count; ++i, ++it ) {
        if ( uintptr_t( it->entry_point ) <= pc ) {
            uintptr_t d = pc - uintptr_t( it->entry_point );
            if ( dist > d ) {
                dist = d;
                ptr = it;
            }
        }
    }
    return ptr;
}

_MD_RegInfo __md_get_register_info( _VM_Frame *frame, uintptr_t pc, const _MD_Function *funMeta )
{
    if ( !frame || !funMeta || !pc )
        return { nullptr, 0 };
    uintptr_t entry = uintptr_t( funMeta->entry_point );
    intptr_t offset = pc - entry;
    if ( offset < 0 || offset > funMeta->inst_table_size )
        return { nullptr, 0 };

    char *base = reinterpret_cast< char * >( frame );
    auto &imeta = funMeta->inst_table[ offset ];
    return { base + imeta.val_offset, imeta.val_width };
}

bool cmpGlobal( const _MD_Global &g, const char *name ) noexcept {
    return std::strcmp( g.name, name ) < 0;
}

bool cmpGlobal( const char *name, const _MD_Global &g ) noexcept {
    return std::strcmp( name, g.name ) < 0;
}

const _MD_Global *__md_get_global_meta( const char *name ) noexcept {
    const auto *end = __md_globals + __md_globals_count;
    auto range = std::equal_range( __md_globals, end, name,
            []( const auto &a, const auto &b ) { return cmpGlobal( a, b ); } );
    if ( range.first == end || range.first == range.second )
        return nullptr;
    return range.first;
}

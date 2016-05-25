#include <cstring>
#include <runtime/divine/metadata.h>

extern const _MD_Function __md_functions[];
extern const int __md_functions_count;

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

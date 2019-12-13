#include <sys/metadata.h>

extern const _MD_Function __md_functions[];
extern const int __md_functions_count;

const _MD_Function *__md_get_pc_meta( _VM_CodePointer f )
{
    for ( int i = 0; i < __md_functions_count; ++i )
        if ( f == __md_functions[ i ].entry_point )
            return __md_functions + i;
    return 0;
}

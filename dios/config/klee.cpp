#define __vm_trace __vm_trace_proto
#include <sys/divm.h>
#undef __vm_trace
extern "C" void __vm_trace( int t, ... );

#include <sys/metadata.h>
#include <stdint.h>
#include <dios/config/context.hpp>
#include <dios/sys/sched_null.hpp>

namespace __dios
{
    using Context = Upcall< WithFS< sched_null< Base > > >;
}

extern const _MD_Function __md_functions[];
extern const int __md_functions_count;

extern "C"
{
    uint8_t *klee_malloc( int );
    uint8_t *klee_realloc( void *, int );
    void klee_free( uint8_t * );
    void klee_assume( int );
    void klee_print_expr( const char *, uint64_t );
    int klee_get_obj_size( void * );
    void klee_abort();
    void klee_silent_exit( int );
    void klee_make_symbolic( void *, size_t, const char * );

    void *__vm_obj_make( int sz, int )
    {
        uint8_t *mem = klee_malloc( 4100 );
        reinterpret_cast< int * >( mem )[0] = sz;
        return mem + sizeof( int );
    }

    void __vm_obj_free( void *ptr ) { klee_free( static_cast< uint8_t * >( ptr ) - 4 ); }
    void __vm_obj_resize( void *optr, int nsz )
    {
        static_cast< int * >( optr )[ -1 ] = nsz;
        klee_assume( nsz <= 4096 );
    }

    int __vm_obj_size( const void *ptr )
    {
        return static_cast< const int * >( ptr )[ -1 ];
    }

    static void *vmreg[ _VM_CR_Last ];

    void __vm_ctl_set( enum _VM_ControlRegister reg, void *val, ... )
    {
        vmreg[ reg ] = val;

        if ( reg == _VM_CR_Frame && val == 0 )
            klee_silent_exit( 0 );
    }

    void *__vm_ctl_get( enum _VM_ControlRegister reg )
    {
        return vmreg[ reg ];
    }

    uint64_t __vm_ctl_flag( uint64_t, uint64_t ) { return 0; }

    void __vm_trace( int t, ... )
    {
        va_list ap;
        va_start( ap, t );
        if ( t == _VM_T_Text )
        {
            const char *txt = va_arg( ap, const char * );
            klee_print_expr( txt, 0 );
        }
    }

    void klee_boot()
    {
        struct _VM_Env env[] = { { "divine.bcname", "kleevm", 7 }, { 0, 0, 0 } };
        __dios::boot< __dios::Context >( env );
        reinterpret_cast< void(*)() >( vmreg[ _VM_CR_Scheduler ] )();
    }

    void __dios_fault( int, const char * )
    {
        klee_abort();
    }

    const _MD_Function *__md_get_pc_meta( _VM_CodePointer f )
    {
        for ( int i = 0; i < __md_functions_count; ++i )
            if ( f == __md_functions[ i ].entry_point )
                return __md_functions + i;
        return 0;
    }

    int __vm_choose( int count, ... )
    {
        int rv;
        klee_make_symbolic( &rv, sizeof(int), "__vm_choose" );
        rv = rv % count;
        klee_print_expr( "__vm_choose", rv );
        return rv;
    }

}

#include <dios/config/common.hpp>

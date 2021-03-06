#define __vm_trace __vm_trace_proto
#include <sys/divm.h>
#undef __vm_trace
extern "C" void __vm_trace( int t, ... );

#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>

extern "C" /* the klee api */
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
}

extern "C" /* platform glue */
{
    void *__vm_obj_make( int sz, int )
    {
        int alloc = sz >= 4096 ? 2 * sz + 2 * sizeof( int ) : 4096 + 2 * sizeof( int );
        uint8_t *mem = klee_malloc( alloc );
        int *meta = reinterpret_cast< int * >( mem );
        meta[ 0 ] = alloc;
        meta[ 1 ] = sz;
        return mem + 2 * sizeof( int );
    }

    void __vm_obj_free( void *ptr ) { klee_free( static_cast< uint8_t * >( ptr ) - 2 * sizeof( int ) ); }
    void __vm_obj_resize( void *optr, int nsz )
    {
        int *meta = static_cast< int * >( optr );
        meta[ -1 ] = nsz;
        if ( meta[ -1 ] > meta[ -2 ] )
            klee_abort();
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
        {
            uint64_t flags = reinterpret_cast< uint64_t >( vmreg[ _VM_CR_Flags ] );
            if ( flags & _VM_CF_Error )
                klee_abort();
            else
                klee_silent_exit( 0 );
        }
    }

    void *__vm_ctl_get( enum _VM_ControlRegister reg )
    {
        return vmreg[ reg ];
    }

    uint64_t __vm_ctl_flag( uint64_t clear, uint64_t set )
    {
        uint64_t flags = reinterpret_cast< uint64_t >( vmreg[ _VM_CR_Flags ] );
        vmreg[ _VM_CR_Flags ] = reinterpret_cast< void * >( ( flags & ~clear ) | set );
        return flags;
    }

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

    int __vm_choose( int count, ... )
    {
        int rv;
        klee_make_symbolic( &rv, sizeof(int), "__vm_choose" );
        rv = rv % count;
        klee_print_expr( "__vm_choose", rv );
        return rv;
    }

    __vm_meta_t __vm_peek( int, uint32_t, int, int )
    {
        return { 0, 0, 0 };
    }

    void __vm_poke( int, uint32_t, int, int, uint32_t ) {}

}

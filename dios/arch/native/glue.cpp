#define __vm_trace __vm_trace_proto
#include <sys/divm.h>
#undef __vm_trace
extern "C" void __vm_trace( int t, ... );

#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>

extern "C" void __dios_native_exit( int );
extern "C" void __dios_native_puts( const char * );
static uint8_t memory[1024 * 1024] __attribute__((aligned(16)));
static int ptr = 16 - sizeof( int );

extern "C" /* platform glue */
{
    void *__vm_obj_make( int sz, int )
    {
        uint8_t *mem = memory + ptr;
        ptr += 4096;
        reinterpret_cast< int * >( mem )[0] = sz;
        return mem + sizeof( int );
    }

    void __vm_obj_free( void * ) { /* munmap( static_cast< uint8_t * >( ptr ) - 4, 4096 ); */ }
    void __vm_obj_resize( void *optr, int nsz )
    {
        static_cast< int * >( optr )[ -1 ] = nsz;
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
                __dios_native_exit( 1 );
            else
                __dios_native_exit( 0 );
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
        if ( t == _VM_T_Text || t == _VM_T_Fault )
        {
            const char *txt = va_arg( ap, const char * );
            __dios_native_puts( txt );
        }
    }

    int __vm_choose( int, ... )
    {
        return 0;
    }

}

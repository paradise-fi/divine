#include <sys/divm.h>
#include <sys/trace.h>
#include <sys/cdefs.h>
#include <string.h>

void __dios_boot( const struct _VM_Env * );
void *klee_malloc( int );
void klee_print_expr( const char *, long );

__link_always void klee_boot( int argc, const char **argv )
{
    struct _VM_Env env[ argc + 1 ];

    env[ 0 ].key = "divine.bcname";
    env[ 0 ].size = strlen( argv[ 0 ] );
    env[ 0 ].value = argv[ 0 ];

    int arg_i = 0, sys_i = 0;

    for ( int i = 1; i < argc; ++i )
    {
        klee_print_expr( "i", i );
        char *key;
        env[ i ].key = key = klee_malloc( 6 );

        if ( argv[ i ][ 0 ] == 's' )
            strcpy( key, "sys." );
        if ( argv[ i ][ 0 ] == 'u' )
            strcpy( key, "arg." );
        key[ 4 ] = '0' + ( argv[ i ][ 0 ] == 's' ? sys_i++ : arg_i++ );
        key[ 5 ] = 0;
        env[ i ].size = strlen( argv[ i ] ) - 2;
        env[ i ].value = argv[ i ] + 2;
    }

    env[ argc ].key = 0;

    __vm_trace( _VM_T_Text, "about to __boot" );
    __dios_boot( env );
    __vm_trace( _VM_T_Text, "about to run the scheduler" );
    ( ( void(*)() ) __vm_ctl_get( _VM_CR_Scheduler ) )();
}

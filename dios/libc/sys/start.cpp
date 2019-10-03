#include <stdlib.h>
#include <pthread.h>
#include <sys/start.h>
#include <sys/metadata.h>

char** environ;
static char **__argv;
char* __progname;

extern char* program_invocation_short_name __attribute__((alias ("__progname")));

extern "C" {
  extern void divine_global_ctors_init();
  extern void divine_global_dtors_fini();
}

namespace {

struct CtorDtorEntry {
    int32_t prio;
    void (*fn)();
    void *ignored; // should be used only by linker to discard entries
};

__invisible void sort( CtorDtorEntry *begin, CtorDtorEntry *end, bool reverse )
{
    for ( auto item = begin; item != end; ++item ) {
        for ( auto j = item; j != begin; j-- ) {
            if ( reverse ? j->prio < (j - 1)->prio : (j - 1)->prio < j->prio )
                break;
            // Swap
            auto tmp = *j;
            *j = *( j - 1 );
            *( j - 1 ) = tmp;
        }
    }
}

__invisible static void run_ctors_dtors( const char *name, bool reverse )
{
    auto *meta = __md_get_global_meta( name );
    if ( !meta )
        return;
    auto *begin = reinterpret_cast< CtorDtorEntry * >( meta->address ),
         *end = begin + meta->size / sizeof( CtorDtorEntry );
    sort( begin, end, reverse );
    for ( ; begin != end; ++begin )
        begin->fn();
}

void freeMainArgs( char** argv ) noexcept {
    char **orig = argv;
    while( *argv ) {
        __vm_obj_free( *argv );
        ++argv;
    }
    __vm_obj_free( orig );
}

} // namespace

extern "C" {
    __attribute__((noinline,weak)) void __lart_globals_initialize() {}
}

void __dios_run_ctors()
{
    divine_global_ctors_init();
}

void __dios_run_dtors()
{
    divine_global_dtors_fini();
}

extern "C" void _exit( int rv )
{
    if ( rv )
    {
        __dios_trace_f( "Non-zero exit code: %d", rv );
        __dios_fault( _DiOS_F_Exit, "exit called with non-zero value" );
    }
    __dios_reschedule();
    __dios_run_dtors();
    __cxa_finalize( 0 );
    __pthread_finalize();
    freeMainArgs( environ );
    freeMainArgs( __argv );
    __dios_exit_process( rv );
    __builtin_unreachable();
}

__attribute__(( __always_inline__ )) int __execute_main( int l, int argc, char **argv, char **envp )
{
    __lart_globals_initialize();
    __pthread_initialize(); // must run before constructors, constructors can
                            // use pthreads (such as pthread_once or thread
                            // local storage)
    __dios_run_ctors();
    int res;
    environ = envp;
    __progname = argv? argv[0] : NULL;
    __argv = argv;
    switch (l)
    {
        case 0:
            res = main();
            break;
        case 2:
            res = main( argc, argv );
            break;
        case 3:
            res = main( argc, argv, envp );
            break;
        default:
            __dios_assert_v( false, "Unexpected prototype of main" );
            res = 256;
    }

    return res;
}

void __dios_start( int l, int argc, char **argv, char **envp )
{
    int res = __execute_main( l, argc, argv, envp );
    exit( res );
}

void __dios_start_synchronous( int l, int argc, char **argv, char **envp )
{
    int res = __execute_main( l, argc, argv, envp );
    if ( res )
    {
        __dios_trace_f( "Non-zero return from setup: %d", res );
        __dios_fault( _DiOS_F_Exit, "setup ended with non-zero value" );
    }
    __dios_yield();
}

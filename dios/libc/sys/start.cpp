#include <stdlib.h>
#include <pthread.h>
#include <sys/start.h>
#include <sys/metadata.h>

char** environ;
char* __progname;

extern char* program_invocation_short_name __attribute__((alias ("__progname")));

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
    run_ctors_dtors( "llvm.global_ctors", false );
}

void __dios_run_dtors()
{
    run_ctors_dtors( "llvm.global_dtors", true );
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

    freeMainArgs( argv );
    freeMainArgs( envp );
    return res;
}

void _start( int l, int argc, char **argv, char **envp )
{
    int res = __execute_main( l, argc, argv, envp );
    exit( res );
}

void _start_synchronous( int l, int argc, char **argv, char **envp )
{
    int res = __execute_main( l, argc, argv, envp );
    if ( res )
    {
        __dios_trace_f( "Non-zero return from setup: %d", res );
        __dios_fault( _DiOS_F_Exit, "setup ended with non-zero value" );
    }
    __dios_yield();
}

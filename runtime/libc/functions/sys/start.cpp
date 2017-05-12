#include <stdlib.h>
#include <sys/start.h>
#include <pthread.h>

#include <divine/metadata.h>

namespace {

struct CtorDtorEntry {
    int32_t prio;
    void (*fn)();
    void *ignored; // should be used only by linker to discard entries
};

template< typename It, typename Comp >
void sort( It begin, It end, Comp cmp ) {
    for ( It item = begin; item != end; ++item ) {
        for ( It j = item; j != begin; j-- ) {
            if ( cmp( *( j - 1), *j ) )
                break;
            // Swap
            auto tmp = *j;
            *j = *( j - 1 );
            *( j - 1 ) = tmp;
        }
    }
}

template< typename Cmp >
static void runCtorsDtors( const char *name, Cmp cmp ) {
    auto *meta = __md_get_global_meta( name );
    if ( !meta )
        return;
    auto *begin = reinterpret_cast< CtorDtorEntry * >( meta->address ),
         *end = begin + meta->size / sizeof( CtorDtorEntry );
    sort( begin, end, cmp );
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

void __dios_run_ctors() {
    runCtorsDtors( "llvm.global_ctors",
            []( CtorDtorEntry &a, CtorDtorEntry &b ) { return a.prio < b.prio; } );
}

void __dios_run_dtors() {
    runCtorsDtors( "llvm.global_dtors",
            []( CtorDtorEntry &a, CtorDtorEntry &b ) { return a.prio > b.prio; } );
}

void _start( int l, int argc, char **argv, char **envp ) {
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask );
    __pthread_initialize(); // must run before constructors, constructors can
                            // use pthreads (such as pthread_once or thread
                            // local storage)
    __dios_run_ctors();
    int res;
    switch (l) {
    case 0:
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_None );
        res = main();
        break;
    case 2:
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_None );
        res = main( argc, argv );
        break;
    case 3:
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_None );
        res = main( argc, argv, envp );
        break;
    default:
        __dios_assert_v( false, "Unexpected prototype of main" );
        res = 256;
    }
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask );

    freeMainArgs( argv );
    freeMainArgs( envp );

    exit( res );
}
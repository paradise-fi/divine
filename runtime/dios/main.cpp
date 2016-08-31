// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>


#include <algorithm>
#include <dios/main.hpp>


int main(...);

namespace __dios {

struct CtorDtorEntry {
    int32_t prio;
    void (*fn)();
    void *ignored; // should be used only by linker to discard entries
};

template< typename Sort >
static void runCtorsDtors( const char *name, Sort sort ) {
    auto *meta = __md_get_global_meta( name );
    if ( !meta )
        return;
    auto *begin = reinterpret_cast< CtorDtorEntry * >( meta->address ),
         *end = begin + meta->size / sizeof( CtorDtorEntry );
    std::sort( begin, end, sort );
    for ( ; begin != end; ++begin )
        begin->fn();
}

static void runCtors() {
    runCtorsDtors( "llvm.global_ctors",
            []( CtorDtorEntry &a, CtorDtorEntry &b ) { return a.prio < b.prio; } );
}

static void runDtors() {
    runCtorsDtors( "llvm.global_dtors",
            []( CtorDtorEntry &a, CtorDtorEntry &b ) { return a.prio > b.prio; } );
}

char *env_to_string( const _VM_Env *env ) noexcept {
    auto arg = static_cast< char * >( __vm_obj_make( env->size + 1 ) );
    memcpy( arg, env->value, env->size );
    arg[ env->size ] = '\0';
    return arg;
}

const _VM_Env *get_env_key( const char* key, const _VM_Env *e ) noexcept {
    for ( ; e->key; e++ ) {
        if ( strcmp( e->key, key ) == 0)
            return e;
    }
    return nullptr;
}

std::pair<int, char**> construct_main_arg( const char* prefix, const _VM_Env *env,
    bool prepend_name) noexcept
{
    int argc = prepend_name ? 1 : 0;
    int pref_len = strlen( prefix );
    const _VM_Env *name = nullptr;
    const _VM_Env *e = env;
    for ( ; e->key; e++ ) {
        if ( memcmp( prefix, e->key, pref_len ) == 0 ) {
            argc++;
        }
        else if ( strcmp( e->key, "divine.bcname" ) == 0 ) {
            __dios_assert_v( !name, "Multiple divine.bcname provided" );
            name = e;
        }
    }
    auto argv = static_cast< char ** >( __vm_obj_make( ( argc + 1 ) * sizeof( char * ) ) );

    char **arg = argv;
    if (prepend_name) {
        __dios_assert_v( name, "Missing binary name: divine.bcname" );
        *argv = env_to_string( name );
        arg++;
    }

    for ( ; env->key; env++ ) {
        if ( memcmp( prefix, env->key, pref_len ) == 0 ) {
            *arg = env_to_string( env );
            arg++;
        }
    }
    *arg = nullptr;

    return { argc, argv };
}

void free_main_arg( char** argv ) noexcept {
    while( *argv ) {
        __vm_obj_free( *argv );
        ++argv;
    }
    __vm_obj_free( argv );
}

} // namespace __dios

void __dios_main( int l, int argc, char **argv, char **envp ) noexcept {
    __vm_mask( 1 );
    __dios_trace_t( "Dios started!" );
    __dios::runCtors();
    int res;
    switch (l) {
    case 0:
        __vm_mask( 0 );
        res = main();
        break;
    case 2:
        __vm_mask( 0 );
        res = main( argc, argv );
        break;
    case 3:
        __vm_mask( 0 );
        res = main( argc, argv, envp );
        break;
    default:
        __dios_assert_v( false, "Unexpected prototype of main" );
        res = 256;
    }
    __vm_mask( 1 );

    if ( res != 0 )
        __vm_fault( ( _VM_Fault ) _DiOS_F_MainReturnValue );

    __dios::runDtors();

    __dios::free_main_arg( argv );
    __dios::free_main_arg( envp );

    __dios_trace_t( "DiOS out!" );
}

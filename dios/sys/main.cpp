// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>


#include <algorithm>
#include <cstdlib>
#include <pthread.h>

#include <sys/metadata.h>
#include <dios/sys/main.hpp>

namespace __dios {

char *env_to_string( const _VM_Env *env ) noexcept {
    auto arg = static_cast< char * >( __vm_obj_make( env->size + 1 ) );
    memcpy( arg, env->value, env->size );
    arg[ env->size ] = '\0';
    return arg;
}

bool getSysOpts( const _VM_Env *e, SysOpts& res ) {
    const char *prefix = "sys.";
    int pref_len = strlen( prefix );
    res.clear();
    for( ; e->key != nullptr; e++ ) {
        if ( memcmp( prefix, e->key, pref_len ) != 0 )
            continue;

        // Add sugar for help command
        std::string_view s( e->value, e->size );
        if ( s == "help") {
            res.emplace_back( "debug", "help" );
            continue;
        }

        auto p = std::find( s.begin(), s.end(), ':' );
        if ( p == s.end() ) {
            __dios_trace_f( "Missing ':' in parameter '%.*s'", e->size, e->value );
            return false;
        }

        if ( std::find( ++p, s.end(), ':') != s.end() ) {
            __dios_trace_f(  "Multiple ':' in parameter '%.*s'", e->size, e->value );
            return false;
        }

        std::string_view val( p, s.end() - p );
        std::string_view key( s.begin(), --p - s.begin() );
        res.emplace_back( key, val );
    }
    return true;
}

std::string_view extractOpt( std::string_view key, SysOpts& opts )
{
    auto r = std::find_if( opts.begin(), opts.end(), [&]( const auto& opt )
    {
        return key == opt.first;
    } );
    if ( r == opts.end() )
        return std::string_view{};
    std::string_view s = r->second;
    opts.erase( r );
    return s;
}

bool extractOpt( std::string_view key, std::string_view value, SysOpts& opts )
{
    auto r = std::find_if( opts.begin(), opts.end(), [&]( const auto& opt ) {
        return key == opt.first && value == opt.second;
    } );
    if ( r == opts.end() )
        return false;
    opts.erase( r );
    return true;
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

void trace_main_arg( int indent, std::string_view name, std::pair<int, char**> args )
{
    __dios_trace_i( indent, "%.*s:", int( name.size() ), name.begin() );
    for (int i = 0; i != args.first; i++ )
        __dios_trace_i( indent + 1, "%d: %s", i, args.second[i] );
}

} // namespace __dios

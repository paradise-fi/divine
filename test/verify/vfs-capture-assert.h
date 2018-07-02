#include <set>
#include <string>

struct Context { const char *msg; };

void passSched()
{
    __vm_ctl_flag( 0, _VM_CF_Cancel );
}

void failSched()
{
    auto ctx = static_cast< Context * >( __vm_ctl_get( _VM_CR_State ) );
    __dios_trace_f( "Failed: %s", ctx->msg );
    __vm_ctl_flag( 0, _VM_CF_Error | _VM_CF_Cancel );
}

#define bAss( expr, ctx, message ) do { \
    if ( !(expr) ) { \
        ctx->msg = message; \
        __vm_ctl_flag( 0, _VM_CF_Error ); \
        __vm_suspend(); \
    } \
} while ( false )

bool isVfs( const char* name ) {
    int l = strlen( name );
    return memcmp( "vfs.", name, 4 ) == 0 &&
           memcmp( ".name", name + l - 5, 5 ) == 0;
}

std::pair< std::set< std::string >, bool > collectVfsNames( const _VM_Env *env ) {
    std::set< std::string > ret;
    bool unique = true;
    for (; env->key; env++ ) {
        if ( !isVfs( env->key ) )
            continue;

        std::string f( env->value, env->value + env->size );
        __dios_trace_f( "C: %s", f.c_str() );
        auto r = ret.insert( f );
        if ( !r.second )
            __dios_trace_f( "Multi-capture of an object: %s", f.c_str() );
        unique = unique && r.second;
    }

    return { ret, unique };
}

int main() {}

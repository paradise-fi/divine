namespace __dios
{
    #include <dios/macro/no_memory_tags>

    #define SYSCALL( name, schedule, ret, arg )                                   \
                                                                                  \
    __trapfn ret SysProxy::name arg noexcept                                      \
    {                                                                             \
        Trap _trap( Trap::schedule );                                             \
        auto ctx = reinterpret_cast< Context * >( __vm_ctl_get( _VM_CR_State ) ); \
        return unpad( ctx, &Context::name, _1, _2, _3, _4, _5, _6 );              \
    };

    #include <sys/syscall.def>

    #undef SYSCALL
    #include <dios/macro/no_memory_tags.cleanup>

}

extern "C" void __attribute__((weak)) __dios_boot( const _VM_Env *env )
{
    __dios::boot< __dios::Context >( env );
}

extern "C" void  __attribute__((weak)) __link_always __boot( const _VM_Env *env )
{
    __dios_boot( env );
    __vm_suspend();
}

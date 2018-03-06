/* TAGS: min c++ */
#include <sys/monitor.h>
#include <dios/core/stdlibwrap.hpp>

volatile int glob;

bool isZero() { return glob == 0; }

struct GlobMon : public __dios::Monitor
{
    void step()
    {
        if ( !isZero() )
            __dios_fault( _DiOS_F_Assert, "..." ); /* ERROR */
    }
};

int main()
{
    for ( int i = 0; i != 2; i++ )
        glob = glob ? 0 : 1;

    __dios::register_monitor( __dios::new_object< GlobMon >() );

    for ( int i = 0; i != 2; i++ )
    {
        glob = glob ? 0 : 1;
        __dios_interrupt();
    }
}

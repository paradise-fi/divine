/* TAGS: min c++ */
/* VERIFY_OPTS: -o nofail:malloc */
#include <sys/monitor.h>
#include <sys/interrupt.h>

volatile int glob;

void flag( const char *msg )
{
    __vm_trace( _VM_T_Fault, msg );
    __vm_ctl_flag( 0, _VM_CF_Error );
}

bool isZero() { return glob == 0; }

struct DummyMon : public __dios::Monitor
{
    void step() { }
};

struct GlobMon : public __dios::Monitor
{
    void step()
    {
        if ( !isZero() )
            flag( "glob is not zero" ); /* ERROR */
    }
};

int main()
{
    for ( int i = 0; i != 2; i++ )
        glob = glob ? 0 : 1;

    __dios::register_monitor( new DummyMon() );
    __dios::register_monitor( new GlobMon() );

    for ( int i = 0; i != 2; i++ )
    {
        glob = glob ? 0 : 1;
        __dios_suspend();
    }
}

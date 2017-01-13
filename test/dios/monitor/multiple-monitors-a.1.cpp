#include <dios.h>

volatile int glob;

bool isZero() { return glob == 0; }

struct DummyMon : public __dios::Monitor {
    void step( __dios::Context& ) { }
};

struct GlobMon : public __dios::Monitor {
    void step( __dios::Context& ) {
        if ( !isZero() )
            __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, _VM_CF_Error ); /* ERROR */
    }
};

int main() {
    for ( int i = 0; i != 2; i++ )
        glob = glob ? 0 : 1;

    __dios_register_monitor( __dios::new_object< DummyMon >() );
    __dios_register_monitor( __dios::new_object< GlobMon >() );

    for ( int i = 0; i != 2; i++ )
        glob = glob ? 0 : 1;
}

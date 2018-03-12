#include <rst/tristate.h>
#include <rst/common.h>

#ifdef __divine__
#include <sys/divm.h>
#else
#include <cstdlib>
#endif

using namespace abstract;

extern "C" {
    bool __abstract_tristate_lower( Tristate * tristate ) _ROOT {
        if ( tristate->value == Tristate::Domain::Unknown ) {
    #ifdef __divine__
            return __vm_choose( 2 );
    #else
            return std::rand() % 2;
    #endif
        }
        return tristate->value;
    }
}


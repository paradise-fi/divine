#include <abstract/tristate.h>
#include <abstract/common.h>

#ifdef __divine__
#include <divine.h>
#else
#include <cstdlib>
#endif

using namespace abstract;

extern "C" {
    Tristate * __abstract_tristate_create() _ROOT {
        return __new< Tristate >( Tristate::Domain::Unknown );
    }

    Tristate * __abstract_tristate_lift( bool b ) _ROOT {
        auto value =  b ? Tristate::Domain::True : Tristate::Domain::False;
        return __new< Tristate >( value );
    }

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


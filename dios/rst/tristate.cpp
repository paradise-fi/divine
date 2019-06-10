#include <rst/tristate.hpp>
#include <rst/common.h>

using __tristate = __dios::rst::abstract::Tristate;

extern "C" {
    _LART_INTERFACE
    bool __lower_tristate( __tristate tr ) { return tr.lower(); }
}

namespace __dios::rst::abstract {

    bool Tristate::lower() const noexcept {
        return static_cast< bool >( *this );
    }

} // namespace __dios::rst::abstract



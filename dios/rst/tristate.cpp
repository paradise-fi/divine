#include <rst/tristate.hpp>
#include <rst/common.hpp>

using __tristate = __dios::rst::abstract::tristate_t;

extern "C" {
    _LART_INTERFACE
    bool __lower_tristate( __tristate tr ) { return tr.lower(); }
}

namespace __dios::rst::abstract {

    bool tristate_t::lower() const noexcept {
        return static_cast< bool >( *this );
    }

} // namespace __dios::rst::abstract



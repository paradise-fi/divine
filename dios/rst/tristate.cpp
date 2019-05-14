#include <rst/tristate.hpp>
#include <rst/common.h>

namespace abstract {
    bool Tristate::lower() const noexcept {
        return static_cast<bool>( *this );
    }
} // namespace abstract



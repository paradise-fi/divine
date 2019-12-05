#pragma once

#ifdef __divine__
#include <sys/divm.h>
#else
#include <cstdlib>
#endif

namespace __dios::rst::abstract {

    /* The tristate_t abstract domain has three values: True, False and Maybe (Unknown).
     * It is used for branching, to decide whether to take the then or else path,
     * or whether to choose non-deterministically (in DIVINE this means split and
     * take both paths).
     * Every other abstract domain needs to define a `to_tristate()` method, which specifies
     * what happens if a value of that abstract type is encountered in an if condition. */
    struct tristate_t {
        enum value_t { False = 0, True = 1, Unknown = 2 };

        constexpr tristate_t( value_t val ) noexcept : value( val ) { }

        bool lower() const noexcept;

        static tristate_t lift_one( bool v ) noexcept
        {
            return v ? tristate_t::value_t::True : tristate_t::value_t::False;
        }

        static tristate_t lift_any() noexcept
        {
            return tristate_t::value_t::Unknown;
        }

        inline operator bool() const noexcept
        {
            if ( value == tristate_t::value_t::Unknown ) {
                #ifdef __divine__
                    return __vm_choose( 2 );
                #else
                    return std::rand() % 2;
                #endif
            }
            return value;
        }

        value_t value;
    };

} // namespace abstract

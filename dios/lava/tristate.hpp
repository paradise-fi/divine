#pragma once

#ifdef __divine__
#include <sys/divm.h>
#else
#include <cstdlib>
#endif

namespace __lava
{
    /* The tristate abstract domain has three values: true, false and maybe
     * (unknown).  It is used for branching, to decide whether to take the then
     * or else path, or whether to choose non-deterministically (in DIVINE this
     * means split and take both paths).
     *
     * Every other abstract domain needs to define a `to_tristate()` method,
     * which specifies what happens if a value of that abstract type is
     * encountered in an if condition. */

    struct tristate
    {
        enum value_t { no = 0, yes = 1, maybe = 2 } value;
        constexpr tristate( value_t val = maybe ) noexcept : value( val ) { }
        explicit constexpr tristate( uint8_t x ) : tristate( value_t( x ) ) {}

        bool lower() const noexcept;
        static tristate lift( bool v ) noexcept { return v ? yes : no; }
        static tristate any() noexcept { return maybe; }

        explicit inline operator bool() const noexcept
        {
            if ( value == maybe )
            {
                #ifdef __divine__
                    return __vm_choose( 2 );
                #else
                    return std::rand() % 2;
                #endif
            }
            return value;
        }
    };

} // namespace abstract

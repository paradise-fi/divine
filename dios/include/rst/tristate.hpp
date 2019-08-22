#pragma once

#ifdef __divine__
#include <sys/divm.h>
#else
#include <cstdlib>
#endif

namespace __dios::rst::abstract {

    /* The Tristate abstract domain has three values: True, False and Maybe (Unknown).
     * It is used for branching, to decide whether to take the then or else path,
     * or whether to choose non-deterministically (in DIVINE this means split and
     * take both paths).
     * Every other abstract domain needs to define a `to_tristate()` method, which specifies
     * what happens if a value of that abstract type is encountered in an if condition. */
    struct Tristate { // TODO why does it not inherit from Base?
        enum Value { False = 0, True = 1, Unknown = 2 };

        Tristate( Value val ) noexcept : value( val ) { }

        bool lower() const noexcept;

        static Tristate lift_one( bool v ) noexcept
        {
            return v ? Tristate::Value::True : Tristate::Value::False;
        }

        static Tristate lift_any() noexcept
        {
            return Tristate::Value::Unknown;
        }

        inline operator bool() const noexcept
        {
            if ( value == Tristate::Value::Unknown ) {
                #ifdef __divine__
                    return __vm_choose( 2 );
                #else
                    return std::rand() % 2;
                #endif
            }
            return value;
        }

        Value value;
    };

} // namespace abstract

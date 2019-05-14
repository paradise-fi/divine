#pragma once

#ifdef __divine__
#include <sys/divm.h>
#else
#include <cstdlib>
#endif

namespace abstract {

    struct Tristate {
        enum Value { False = 0, True = 1, Unknown = 2 };

        Tristate( Value val ) : value( val ) { }

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

#pragma once
#include <rst/common.h>

namespace abstract {

struct Tristate {
    enum Value { False = 0, True = 1, Unknown = 2 };

    Tristate( Value val ) : value( val ) { }

    Value value;
};

extern "C" {
    bool __tristate_lower( Tristate tristate ) _ROOT _NOTHROW;
    Tristate __tristate_lift( bool v ) _ROOT _NOTHROW;
}

} // namespace abstract

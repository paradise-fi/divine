// -*- C++ -*- (c) 2011 Jan Kriho

#ifndef DIVINE_DVE_ERROR_H
#define DIVINE_DVE_ERROR_H

#include <stdint.h>
#include <iostream>

namespace divine {
namespace dve {
    
struct ErrorState {
    union {
        struct {
            uint8_t overflow:1;
            uint8_t arrayCheck:1;
            uint8_t divByZero:1;
            uint8_t other:1;
            uint8_t fill:4;
        };
        uint8_t error;
    };
    static const ErrorState e_overflow;
    static const ErrorState e_arrayCheck;
    static const ErrorState e_divByZero;
    static const ErrorState e_other;
    static const ErrorState e_none;
};

std::ostream &operator<<( std::ostream &o, const ErrorState &err );

struct StateFlags {
    union {
        struct {
            uint8_t commited:1;
            uint8_t commited_dirty:1;
            uint8_t fill:6;
        };
        uint8_t flags;
    };
    static const StateFlags f_commited;
    static const StateFlags f_commited_dirty;
    static const StateFlags f_none;
};

std::ostream &operator<<( std::ostream &o, const StateFlags &err );

}
}


#endif
// -*- C++ -*- (c) 2011 Jan Kriho

#ifndef DIVINE_DVE_ERROR_H
#define DIVINE_DVE_ERROR_H

#include <cstdint>
#include <iostream>

namespace divine {
namespace dve {

struct ErrorState {
    uint8_t error;

    inline bool overflow() const {
        return error & i_overflow;
    }

    inline bool arrayCheck() const {
        return error & i_arrayCheck;
    }

    inline bool divByZero() const {
        return error & i_divByZero;
    }

    inline bool other() const {
        return error & i_other;
    }

    inline bool padding() const {
        return error & i_padding;
    }

    inline void operator|=( ErrorState err ) {
        this->error |= err.error;
    }

    inline void operator|=( uint8_t err ) {
        this->error |= err;
    }

    inline ErrorState( const uint8_t err ) : error( err ) {}
    inline ErrorState( const ErrorState & err ) : error( err.error ) {}
    inline ErrorState() : error( 0 ) {}

    static const ErrorState e_overflow;
    static const ErrorState e_arrayCheck;
    static const ErrorState e_divByZero;
    static const ErrorState e_other;
    static const ErrorState e_none;

    static const uint8_t i_overflow = 1;
    static const uint8_t i_arrayCheck = 2;
    static const uint8_t i_divByZero = 4;
    static const uint8_t i_other = 8;
    static const uint8_t i_none = 0;
    static const uint8_t i_padding = ~( i_overflow | i_arrayCheck
                                        | i_divByZero | i_other );
};

std::ostream &operator<<( std::ostream &o, const ErrorState &err );

struct StateFlags {
    union {
        struct {
            uint8_t commited:1;
            uint8_t commited_dirty:1;
            uint8_t fill:6;
        } f;
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

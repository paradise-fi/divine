#ifndef __ABSTRACT_TRISTATE_H_
#define __ABSTRACT_TRISTATE_H_

#ifdef __divine__
#include <divine.h>
#else
#include <cstdlib>
#endif

#include "common.h"

#define _ROOT __attribute__((__annotate__("brick.llvm.prune.root")))

namespace abstract {

struct Tristate {
    enum Domain { False = 0, True = 1, Unknown = 2 };
    Domain value;
};

Tristate * __abstract_tristate_construct( Tristate::Domain value );
Tristate * __abstract_tristate_construct();
Tristate * __abstract_tristate_construct( bool b );

Tristate * __abstract_tristate_negate( Tristate * t );
Tristate * __abstract_tristate_and( Tristate * a, Tristate * b );
Tristate * __abstract_tristate_or( Tristate * a, Tristate * b );

extern "C" {
    Tristate * __abstract_tristate_create() _ROOT;
    Tristate * __abstract_tristate_lift( bool b ) _ROOT;
    bool __abstract_tristate_lower( Tristate * tristate ) _ROOT;
}

} // namespace abstract

#endif //__ABSTRACT_TRISTATE_H_

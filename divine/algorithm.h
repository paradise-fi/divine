// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/config.h>

#ifndef DIVINE_ALGORITHM_H
#define DIVINE_ALGORITHM_H

namespace divine {
namespace algorithm {

inline int workerCount( Config *c ) {
    if ( !c )
        return 1;
    return c->workers();
}

struct Algorithm
{
    void resultBanner( bool valid ) {
        std::cerr << " ===================================== " << std::endl
                  << ( valid ?
                     "       Accepting cycle NOT found       " :
                     "         Accepting cycle FOUND         " )
                  << std::endl
                  << " ===================================== " << std::endl;
    }
};

}
}

#endif

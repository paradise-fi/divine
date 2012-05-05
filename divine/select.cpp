// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <divine/select.h>

namespace divine {

algorithm::Algorithm *select( Meta &m )
{
    switch( m.algorithm.algorithm ) {
        case meta::Algorithm::Reachability:
            m.algorithm.name = "Reachability";
            return selectReachability( m );
        case meta::Algorithm::Metrics:
            m.algorithm.name = "Metrics";
            return selectMetrics( m );

        case meta::Algorithm::Owcty:
            m.algorithm.name = "OWCTY";
            m.input.propertyType = meta::Input::Neverclaim; // XXX
            return selectOWCTY( m );
        case meta::Algorithm::Map:
            m.algorithm.name = "MAP";
            m.input.propertyType = meta::Input::Neverclaim; // XXX
            return selectMAP( m );
        case meta::Algorithm::Ndfs:
            m.algorithm.name = "Nested DFS";
            m.input.propertyType = meta::Input::Neverclaim; // XXX
            return selectNDFS( m );

        default:
            return 0;
    }
}

}

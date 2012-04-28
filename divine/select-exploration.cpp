#include <divine/select.h>

#include <divine/algorithm/metrics.h>
// #include <divine/algorithm/reachability.h>
// #include <divine/algorithm/compact.h>

namespace divine {

algorithm::Algorithm *selectExploration( Meta &meta ) {
    switch( meta.algorithm.algorithm ) {
        case meta::Algorithm::Metrics:
            meta.algorithm.name = "Metrics";
            return selectGraph< algorithm::Metrics >( meta );
#if 0
        case meta::Algorithm::Reachability:
            meta.algorithm.name = "Reachability";
            return selectGraph< algorithm::Reachability >( meta );
#ifndef O_SMALL
        case meta::Algorithm::Compact:
            meta.algorithm.name = "Compact";
            return selectGraph< algorithm::Compact >( meta );
#endif
#endif
        default:
            return NULL;
    }
}

}

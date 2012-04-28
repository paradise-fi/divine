#if 0
#include <divine/select.h>

#ifndef O_SMALL
#include <divine/algorithm/probabilistic.h>
#endif

namespace divine {

algorithm::Algorithm *selectProbabilistic( Meta &meta ) {
    switch( meta.algorithm.algorithm ) {
#ifndef O_SMALL
        case meta::Algorithm::Probabilistic:
            assert( meta.input.model.substr( meta.input.model.rfind( '.' ) ) == ".compact" );
            meta.algorithm.name = "Probabilistic";
            return new algorithm::Probabilistic<
                algorithm::NonPORGraph< generator::Compact > >( meta );
#endif
        default:
            return NULL;
    }
}

}
#endif

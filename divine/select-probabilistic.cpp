#include <divine/select.h>
#include <divine/algorithm/probabilistic.h>

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

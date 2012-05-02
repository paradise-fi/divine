#if 0
#ifndef O_SMALL
#include <divine/select.h>
#include <divine/algorithm/probabilistic.h>

namespace divine {

algorithm::Algorithm *selectProbabilistic( Meta &meta ) {
    assert( meta.input.model.substr( meta.input.model.rfind( '.' ) ) == ".compact" );
    return new algorithm::Probabilistic<
        algorithm::NonPORGraph< generator::Compact > >( meta );
}

}
#endif
#endif

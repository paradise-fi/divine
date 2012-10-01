#include <divine/algorithm/metrics.h>
#include <divine/instances/instantiate.h>

namespace divine {

algorithm::Algorithm *selectMetrics( Meta &meta ) {
    return selectGraph< algorithm::Metrics >( meta );
}

}

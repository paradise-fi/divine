#include <divine/algorithm/metrics.h>
#include <divine/select.h>

namespace divine {

algorithm::Algorithm *selectMetrics( Meta &meta ) {
    return selectGraph< algorithm::Metrics >( meta );
}

}

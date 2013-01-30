#include <divine/algorithm/tracer.h>
#include <divine/instances/instantiate.h>

namespace divine {

algorithm::Algorithm *selectTracer( Meta &meta ) {
    return selectGraph< algorithm::Tracer >( meta );
}

}

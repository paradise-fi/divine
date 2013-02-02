#include <divine/algorithm/simulate.h>
#include <divine/instances/instantiate.h>

namespace divine {

algorithm::Algorithm *selectSimulate( Meta &meta ) {
    return selectGraph< algorithm::Simulate >( meta );
}

}

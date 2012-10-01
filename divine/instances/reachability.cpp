#include <divine/algorithm/reachability.h>
#include <divine/instances/instantiate.h>

namespace divine {

algorithm::Algorithm *selectReachability( Meta &meta ) {
    return selectGraph< algorithm::Reachability >( meta );
}

}

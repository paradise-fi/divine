#include <divine/algorithm/reachability.h>
#include <divine/select.h>

namespace divine {

algorithm::Algorithm *selectReachability( Meta &meta ) {
    return selectGraph< algorithm::Reachability >( meta );
}

}

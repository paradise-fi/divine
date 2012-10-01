#include <divine/algorithm/map.h>
#include <divine/instances/instantiate.h>

namespace divine {

algorithm::Algorithm *selectMAP( Meta &meta ) {
    return selectGraph< algorithm::Map >( meta );
}

}

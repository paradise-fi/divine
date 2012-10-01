#include <divine/algorithm/owcty.h>
#include <divine/instances/instantiate.h>

namespace divine {

algorithm::Algorithm *selectOWCTY( Meta &meta ) {
    return selectGraph< algorithm::Owcty >( meta );
}

}

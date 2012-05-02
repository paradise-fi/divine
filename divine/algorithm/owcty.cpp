#include <divine/algorithm/owcty.h>
#include <divine/select.h>

namespace divine {

algorithm::Algorithm *selectOWCTY( Meta &meta ) {
    return selectGraph< algorithm::Owcty >( meta );
}

}

#include <divine/algorithm/map.h>
#include <divine/select.h>

namespace divine {

algorithm::Algorithm *selectMAP( Meta &meta ) {
    return selectGraph< algorithm::Map >( meta );
}

}

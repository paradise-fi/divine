#include <divine/algorithm/nested-dfs.h>
#include <divine/select.h>

namespace divine {

algorithm::Algorithm *selectNDFS( Meta &meta ) {
    return selectGraph< algorithm::NestedDFS >( meta );
}

}

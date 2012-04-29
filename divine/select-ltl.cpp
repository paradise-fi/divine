#include <divine/select.h>

#include <divine/algorithm/owcty.h>
#include <divine/algorithm/map.h>
#include <divine/algorithm/nested-dfs.h>

namespace divine {

algorithm::Algorithm *selectLtl( Meta &meta ) {
    switch( meta.algorithm.algorithm ) {
        case meta::Algorithm::Owcty:
            meta.algorithm.name = "OWCTY";
            meta.input.propertyType = meta::Input::Neverclaim; // XXX
            return selectGraph< algorithm::Owcty >( meta );
#ifndef O_SMALL
        case meta::Algorithm::Map:
            meta.algorithm.name = "MAP";
            meta.input.propertyType = meta::Input::Neverclaim; // XXX
            return selectGraph< algorithm::Map >( meta );
#endif
        case meta::Algorithm::Ndfs:
            meta.algorithm.name = "Nested DFS";
            meta.input.propertyType = meta::Input::Neverclaim; // XXX
            return selectGraph< algorithm::NestedDFS >( meta );
        default:
            return NULL;
    }
}

}

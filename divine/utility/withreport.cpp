#include <divine/utility/withreport.h>

namespace divine {

std::ostream &operator<<( std::ostream &o, const WithReport &wr ) {
    const auto &rep = wr.report();
    for ( auto x : rep ) {
        if ( x.key == "" )
            o << std::endl;
        else
            o << x.key << ": " << x.value << std::endl;
    }
    return o;
}

}

#include <divine/dve/symtab.h>

namespace divine {
namespace dve {
    
std::ostream &operator<<( std::ostream &os, Symbol s )
{
    return os << s.id;
}

}
}
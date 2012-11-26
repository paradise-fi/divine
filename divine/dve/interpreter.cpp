#include <divine/dve/interpreter.h>

namespace divine {
namespace dve {

std::ostream & dumpChannel( std::ostream &os, Channel * chan, char * data) {
    return chan->dump( os, data );
}

std::ostream & Channel::dump( std::ostream &os, char * data )
{
    os << this->name << " = [";
    for ( size_t i = 0; i < this->count( data ); i++ ) {
        char * _item = this->item( data, i );
        if (i != 0)
            os << ", ";
        if ( this->components.size() > 1 )
            os << "{";
        for ( auto it = this->components.begin();
              it != this->components.end(); it++ )
        {
            if (it != this->components.begin())
                os << ", ";
            os << it->deref( _item, 0 );
        }
        if ( this->components.size() > 1 )
            os << "}";
    }
    os << "], ";
    return os;
}

}
}
#pragma once

#include <divine/cc/filetype.hpp>

namespace divine {
namespace cc {

struct Options
{
    bool dont_link;
    bool verbose;
    Options() : Options( false, true ) {}
    Options( bool dont_link, bool verbose ) : dont_link( dont_link ), verbose( verbose ) {}
};

}
}

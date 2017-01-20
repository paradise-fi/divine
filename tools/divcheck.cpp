#include <tools/divcheck.hpp>

int main( int argc, const char **argv )
{
    if ( argc != 2 )
    {
        std::cerr << "usage: divcheck ./script" << std::endl;
        return 1;
    }

    auto script = brick::fs::readFile( argv[1] );
    divcheck::execute( script, []( auto & ) {}, []( auto &cmd ) { cmd.run(); } );
}

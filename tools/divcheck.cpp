#include <tools/divcheck.hpp>

int main( int argc, const char **argv )
{
    if ( argc != 2 )
    {
        std::cerr << "usage: divcheck ./script" << std::endl;
        return 1;
    }

    auto script = brick::fs::readFile( argv[1] );
    ::chdir( brick::string::dirname( argv[1] ).c_str() );
    try {
        divcheck::execute( script );
    } catch ( divcheck::Wrong &e ) {
        std::cerr << "### " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

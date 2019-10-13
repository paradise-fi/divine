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
    divine::ui::setup_death();
    divcheck::execute( script );
    return 0;
}

#include <tools/divcheck.hpp>

int main( int argc, const char **argv )
{
    if ( argc != 2 )
    {
        std::cerr << "usage: divcheck ./script" << std::endl;
        return 1;
    }

    auto script = brq::read_file( argv[ 1 ] );
    brq::change_dir wd( brq::dirname( argv[ 1 ] ) );
    divine::ui::setup_death();
    divcheck::execute( script );
    return 0;
}

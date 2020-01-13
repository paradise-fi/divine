#include <tools/divcheck.hpp>
#include <divine/ui/sysinfo.hpp>

int main( int argc, const char **argv ) try
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
catch ( divine::ui::ResourceLimit &e )
{
    std::cerr << "E: " << e.what() << std::endl;
    return 202; /* indicate resource exhausted */
}
catch ( divcheck::Wrong &w )
{
    std::cerr << "E: " << w.what() << std::endl;
    return 1;
}

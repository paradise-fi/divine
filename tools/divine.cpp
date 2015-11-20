// -*- C++ -*- (c) 2015 Petr Rockai <m@fixp.eu>

#include <divine/ui/cli.hpp>
#include <divine/ui/curses.hpp>

#ifdef __unix__
#include <signal.h>
#endif

std::shared_ptr< divine::ui::Interface > _ui = 0;

#ifdef __unix__
void handler( int s )
{
    signal( s, SIG_DFL );
    if ( _ui )
        _ui->signal( s );
    raise( s );
}
#endif

void panic()
{
    try
    {
        if ( std::current_exception() )
            std::rethrow_exception( std::current_exception() );
        std::cerr << "E: std::terminate() called without an active exception" << std::endl;
        std::abort();
    }
    catch ( std::exception &e )
    {
        if ( _ui )
            _ui->exception( e );
        std::cerr << "E: " << e.what() << std::endl;
        std::abort();
    }
}

int main( int argc, const char **argv )
{
#ifdef __unix__
    for ( int i = 0; i <= 32; ++i )
        if ( i == SIGCHLD || i == SIGWINCH || i == SIGURG )
            continue;
        else
            signal( i, handler );
#endif
    std::set_terminate( panic );
    _ui = std::make_shared< divine::ui::CLI >( argc, argv );
    _ui = _ui->resolve();
    return _ui->main();
}

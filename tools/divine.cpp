// -*- C++ -*- (c) 2015 Petr Rockai <m@fixp.eu>

#include <divine/ui/cli.hpp>
#include <divine/ui/curses.hpp>
#include <brick-fs>

#if OPT_Z3
#include <z3++.h>
#endif

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
#if OPT_Z3
    catch ( z3::exception &e )
    {
        std::cerr << "E: " << e.msg() << std::endl;
        std::abort();
    }
#endif
    catch ( ... ) {
        std::cerr << "E: unknown exception" << std::endl;
        std::abort();
    }
}

int main( int argc, const char **argv )
{
#ifdef __unix__
    for ( int i = 0; i <= 32; ++i )
        if ( i == SIGCHLD || i == SIGWINCH || i == SIGURG || i == SIGTSTP || i == SIGCONT ||
             i == SIGKILL )
            continue;
        else
            signal( i, handler );
#endif
    std::set_terminate( panic );

    auto args = brick::cmd::from_argv( argc, argv );
    std::string progName = brick::fs::basename( argv[ 0 ] );
    if ( progName == "divine-cc" )
        args.insert( args.begin(), "divinecc" );
    else if ( progName == "divine-ld" )
        args.insert( args.begin(), "divineld" );

    _ui = divine::ui::make_cli( args )->resolve();
    return _ui->main();
}

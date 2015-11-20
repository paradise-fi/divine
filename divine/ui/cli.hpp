#pragma once

#include <divine/ui/common.hpp>
#include <divine/ui/curses.hpp>
#include <brick-cmd>
#include <brick-fs>

namespace divine {
namespace ui {

namespace cmd = brick::cmd;

struct Common
{
    std::string bitcode;
};

struct Verify : Common {};
struct Draw   : Common {};

struct CLI : Interface
{
    bool _batch;
    std::vector< std::string > _args;

    CLI( int argc, const char **argv )
        : _batch( true )
    {
        for ( int i = 1; i < argc; ++i )
            _args.emplace_back( argv[ i ] );
    }

    auto parse()
    {
        auto v = cmd::make_validator()->
                 add( "file", []( std::string s )
                      {
                          if ( !brick::fs::access( s, F_OK ) )
                              throw std::runtime_error( "file " + s + " does not exist");
                          if ( !brick::fs::access( s, R_OK ) )
                              throw std::runtime_error( "file " + s + " is not readable");
                          return s;
                      } );
        auto common = cmd::make_option_set< Common >( v )
                      .add( "{file}", &Common::bitcode, std::string( "the bitcode file to load" ) );
        auto p = cmd::make_parser( v )
                 .add< Verify >( common, cmd::Inherited() )
                 .add< Draw >( common, cmd::Inherited() );
        return p.parse( _args.begin(), _args.end() );
    }

    std::shared_ptr< Interface > resolve() override
    {
        if ( _batch )
            return Interface::resolve();
        else
            return std::make_shared< ui::Curses >( /* ... */ );
    }

    virtual int main() override
    {
        auto r = parse().match(
            [&]( Verify v ) { std::cerr << "verify: " << v.bitcode << std::endl; },
            [&]( Draw d ) { std::cerr << "draw: " << d.bitcode << std::endl; }
        );
        if ( r.isNothing() )
            throw std::runtime_error( "error parsing command line" );
        return 0;
    }
};

}
}

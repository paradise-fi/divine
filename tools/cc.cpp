// -*- C++ -*- (c) 2016 Vladimír Štill

#include <divine/compile/cc.h>
#include <brick-query.h>
#include <brick-string.h>

#include <iostream>

using brick::string::startsWith;

std::string substrAfter( std::string str, char c ) {
    auto pos = str.find( c );
    return pos == std::string::npos ? "" : str.substr( pos + 1 );
}

int main( int argc, char **argv ) {
    using namespace divine::cc;
    if ( argc == 1 ) {
        std::cerr << "USAGE: divinecc [options] file" << std::endl;
        std::exit( 2 );
    }

    Compile::Options driverOpts;
    std::vector< std::string > opts;
    std::vector< std::string > files;
    std::vector< std::string > allowed;
    std::string output;
    bool libsOnly = false;

    std::string includeNext = "";
    bool outputNext = false;
    for ( std::string arg : brick::query::range( argv + 1, argv + argc ) ) {
        if ( arg == "--libraries-only" ) {
            libsOnly = true;
            driverOpts.emplace_back( Compile::LibsOnly() );
        } else if ( startsWith( arg, "--precompiled=" ) ) {
            driverOpts.emplace_back( Compile::Precompiled( substrAfter( arg, '=' ) ) );
        } else if ( arg == "--disable-vfs" ) {
            driverOpts.emplace_back( Compile::DisableVFS() );
        } else if ( startsWith( arg, "--stdin=" ) ) {
            driverOpts.emplace_back( Compile::VFSInput( substrAfter( arg, '=' ) ) );
        } else if ( startsWith( arg, "--snapshot=" ) ) {
            driverOpts.emplace_back( Compile::VFSSnapshot( substrAfter( arg, '=' ) ) );
        } else if ( outputNext ) {
            outputNext = false;
            output = arg;
        } else if ( includeNext.size() ) {
            allowed.emplace_back( arg );
            opts.emplace_back( includeNext + arg );
            includeNext = "";
        } else if ( startsWith( arg, "-I" ) || startsWith( arg, "-isystem" ) ) {
            if ( arg == "-I" || arg == "-isystem" )
                includeNext = arg;
            else {
                auto ipath = arg.substr( arg.size() );
                allowed.emplace_back( ipath );
                opts.emplace_back( arg );
            }
        } else if ( startsWith( arg, "-o" ) ) {
            if ( arg == "-o" )
                outputNext = true;
            else
                output = arg.substr( 2 );
        } else if ( startsWith( arg, "-" ) )
            opts.emplace_back( arg );
        else
            files.emplace_back( arg );
    }

    Compile driver( driverOpts );
    for ( auto &d : allowed )
        driver.addDirectory( d );

    std::cout << "opts: ";
    std::copy( opts.begin(), opts.end(), std::ostream_iterator< std::string >( std::cout, " " ) );
    std::cout << "\nfiles: ";
    std::copy( files.begin(), files.end(), std::ostream_iterator< std::string >( std::cout, " " ) );
    std::cout << std::endl;

    if ( files.empty() && !libsOnly ) {
        std::cerr << "ERROR: no imput files given" << std::endl;
        std::exit( 1 );
    }
    if ( libsOnly && !files.empty() ) {
        std::cerr << "ERROR: you can't specify files in --libraries-only is given" << std::endl;
        std::exit( 1 );
    }
    if ( files.size() > 1 &&
            std::find_if( opts.begin(), opts.end(), []( auto &str ) { return startsWith( str, "-x" ); } )
                != opts.end() )
    {
        std::cerr << "ERROR: divinecc currently can't handle combination of -x and multiple files" << std::endl;
        std::exit( 1 );
    }

    if ( output.empty() ) {
        if ( libsOnly )
            output = "libdivinert.bc";
        else {
            output = files.front();
            auto pos = output.rend() - std::find( output.rbegin(), output.rend(), '.' );
            output = output.substr( 0, pos - 1 ) + ".bc";
        }
    }

    for ( auto &x : files )
        driver.compileAndLink( x, opts );

    driver.writeToFile( output );
}

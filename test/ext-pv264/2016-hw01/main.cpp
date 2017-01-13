/* VERIFY_OPTS: -o nofail:malloc */
/* PROGRAM_OPTS: --use-colour no */
#include <vector>
#include <iostream>
#include <string>
#include "lazy.h"

struct X {
    X( const char *msg ) {
        std::cout << msg << std::endl;
    }
};

void foo() {
    static X x( "foo" );
}

int main() {

    const std::vector< int > x{ 1,2,3,4,5,6,7,8,9 };
    const std::vector< int > y{ 1,1,1,1,1,1,1,1,1 };
    const std::vector< std::string > t{ "ahoj", "karle", "tele" };
    std::vector< const std::string * > pt;
    for ( const auto &i : t ) {
        pt.push_back( &i );
    }

    auto p = lazy::map( x.begin(), x.end(), []( int x ) -> double { return x + 0.5; } );

    for ( auto i : p ) {
        std::cout << i << std::endl;
    }
    std::cout << "---" << std::endl;

    auto f = lazy::filter( p.begin(), p.end(), []( double x ) { return x > 3 && x < 7; } );
    for ( auto i : f ) {
        std::cout << i << std::endl;
    }
    std::cout << "---" << std::endl;
    auto z = lazy::zip( f.begin(), f.end(), x.begin(), x.end(), []( double x, int y ) {
        return x + y;
    } );
    for ( auto i : z ) {
        std::cout << i << std::endl;
    }
    std::cout << "---" << std::endl;

    auto u = lazy::unique( y.begin(), y.end() );
    for ( auto i : u ) {
        std::cout << i << std::endl;
    }
    std::cout << "---" << std::endl;

    for ( auto i : lazy::unique( z.begin(), z.end() ) ) {
        std::cout << i << std::endl;
    }

    auto ff = lazy::filter( t.begin(), t.end(), []( const std::string &s ) {
        return s.size() == 4;
    } );
    auto mm = lazy::map( ff.begin(), ff.end(), []( const std::string &s ) {
        return "[" + s + "]";
    } );
    for ( auto i = mm.begin(); i != mm.end(); ++i ) {
        std::cout << i->size() << ": " << *i << std::endl;
    }

    return 0;
}

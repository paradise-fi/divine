/* VERIFY_OPTS: -o nofail:malloc */
/* PROGRAM_OPTS: --use-colour no */
#include "lazy.h"
#include "common.h"

#include <string>
#include <vector>

TEST_CASE( "complex" ) {
    std::vector< std::string > words{
        "hi",
        "hello",
        "world",
        "terrible",
        "a",
        "I",
        "piggy",
        "car",
        "left",
        "a"
    };

    SECTION( "chain1" ) {
        auto f = lazy::filter( words.begin(), words.end(), []( const std::string &w ) {
            return w.size() == 5;
        } );
        auto m = lazy::map( f.begin(), f.end(), []( const std::string &w ) {
            return w.size();
        } );
        auto u = lazy::unique( m.begin(), m.end() );

        ::tests::check( u.begin(), u.end(), { 5 } );
    }


    SECTION( "chain2" ) {
        using namespace std::literals;
        auto m = lazy::map( words.begin(), words.end(), []( const std::string &w ) {
            return w.size();
        } );
        auto u = lazy::unique( m.begin(), m.end() );

        auto z = lazy::zip( u.begin(), u.end(),
                            words.begin(), words.end(),
                            []( size_t s, const std::string &w ) {
            if ( s != w.size() )
                return "--"s;
            return w;
        } );
        ::tests::check( z.begin(), z.end(), { "hi", "hello", "--", "--", "--", "--" } );
    }
    SECTION( "chain3" ) {
        auto m = lazy::map( words.begin(), words.end(), []( const std::string &w ) {
            return w.size();
        } );
        auto z = lazy::zip( m.begin(), m.end(),
                            words.begin(), words.end(),
                            []( size_t s, const std::string &w ) {
            return w + " " + std::to_string( s );
        } );
        SECTION( "3" ) {
            auto f = lazy::filter( z.begin(), z.end(), []( const std::string &s ) {
                return s.size() == 3;
            } );
            ::tests::check( f.begin(), f.end(), { "a 1", "I 1", "a 1" } );
        }
        SECTION( "5" ) {
            auto f = lazy::filter( z.begin(), z.end(), []( const std::string &s ) {
                return s.size() == 5;
            } );
            ::tests::check( f.begin(), f.end(), { "car 3" } );
        }
        SECTION( "10" ) {
            auto f = lazy::filter( z.begin(), z.end(), []( const std::string &s ) {
                return s.size() == 10;
            } );
            ::tests::check( f.begin(), f.end(), { "terrible 8" } );
        }
    }

}



// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
#pragma once

#include <string>
#include <vector>
#include <iostream>

namespace divine {

struct ReportLine {
    std::string key;
    std::string value;

    ReportLine( std::string key, std::string value ) : key( key ), value( value ) { }
};

struct WithReport {
    virtual std::vector< ReportLine > report() const = 0;

    template< typename... Ts >
    static std::vector< ReportLine > merge( const Ts &...ts ) {
        std::vector< ReportLine > vec;
        auto inserter = std::back_inserter( vec );
        _merge( inserter, ts... );
        return vec;
    }

    virtual ~WithReport() { }

  private:
    using BIV = std::back_insert_iterator< std::vector< ReportLine > >;
    static void _merge( BIV & ) { }
    template< typename... Ts >
    static void _merge( BIV &target, const std::vector< ReportLine > &source, const Ts &...ts ) {
        std::copy( source.begin(), source.end(), target );
        _merge( target, ts... );
    }
    template< typename... Ts >
    static void _merge( BIV &target, const ReportLine &source, const Ts &...ts ) {
        target = source;
        _merge( target, ts... );
    }
    template< typename... Ts >
    static void _merge( BIV &target, const WithReport &source, const Ts &...ts ) {
        _merge( target, source.report(), ts... );
    }
};

struct Empty : WithReport {
    std::vector< ReportLine > report() const override {
        return { { "", "" } };
    }
};

static inline std::ostream &operator<<( std::ostream &o, const WithReport &wr )
{
    const auto &rep = wr.report();
    for ( auto x : rep )
    {
        if ( x.key == "" )
            o << std::endl;
        else
            o << x.key << ": " << x.value << std::endl;
    }
    return o;
}

}


// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
#include <string>
#include <vector>
#include <iostream>

#ifndef DIVINE_UTILITY_WITHREPORT
#define DIVINE_UTILITY_WITHREPORT

namespace divine {

struct ReportPair {
    std::string key;
    std::string value;

    ReportPair( std::string key, std::string value ) : key( key ), value( value ) { }
};

struct WithReport {
    virtual std::vector< ReportPair > report() const = 0;

    template< typename... Ts >
    static std::vector< ReportPair > merge( const Ts &...ts ) {
        std::vector< ReportPair > vec;
        auto inserter = std::back_inserter( vec );
        _merge( inserter, ts... );
        return vec;
    }

    virtual ~WithReport() { }

  private:
    using BIV = std::back_insert_iterator< std::vector< ReportPair > >;
    static void _merge( BIV & ) { }
    template< typename... Ts >
    static void _merge( BIV &target, const std::vector< ReportPair > &source, const Ts &...ts ) {
        std::copy( source.begin(), source.end(), target );
        _merge( target, ts... );
    }
    template< typename... Ts >
    static void _merge( BIV &target, const ReportPair &source, const Ts &...ts ) {
        target = source;
        _merge( target, ts... );
    }
    template< typename... Ts >
    static void _merge( BIV &target, const WithReport &source, const Ts &...ts ) {
        _merge( target, source.report(), ts... );
    }
};

struct Empty : WithReport {
    std::vector< ReportPair > report() const override {
        return { { "", "" } };
    }
};

std::ostream &operator<<( std::ostream &, const WithReport & );

}
#endif

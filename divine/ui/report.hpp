// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>
//             (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <iomanip>
#include <sstream>
#include <string>
#include <fstream>
#include <type_traits>
#include <memory>

#include <brick-types>

#include <divine/ui/version.hpp>
// #include <divine/ui/meta.h>
#include <divine/ui/sysinfo.hpp>

#ifndef DIVINE_REPORT_H
#define DIVINE_REPORT_H

namespace divine {

struct Report
{
    Report() : _signal( 0 ), _finished( false ), _dumped( false )
    { }

    virtual void signal( int s ) {
        _finished = false;
        _signal = s;
    }

    virtual void finished() {
        _finished = true;
    }

    virtual void execCommand( const std::string &ec ) {
        _execCommand = ec;
    }

    virtual void doFinal() = 0;

    struct Merged : WithReport {
        const Report &r;

        Merged( const Report &r ) : r( r ) { }

        std::vector< ReportLine > report() const override;
    };

    Merged mergedReport() const {
        return Merged( *this );
    }

    static std::string mangle( std::string str );

    template< typename Rep, typename... Ts >
    static std::shared_ptr< Report > get( Ts &&... ts ) {
        return _get< Rep >( brick::types::Preferred(), std::forward< Ts >( ts )... );
    }

    virtual ~Report() {}

  private:
    sysinfo::Info _sysinfo;
    std::string _execCommand;

    int _signal;
    bool _finished, _dumped;

    template< typename Rep >
    static std::shared_ptr< Rep > declcheck( std::shared_ptr< Rep > ) {
        static_assert( std::is_base_of< Report, Rep >::value,
                       "Required report does not inherit from Report." );
        UNREACHABLE( "declcheck" );
    }

    template< typename Rep, typename... Ts >
    static auto _get( brick::types::Preferred, Ts &&... ts ) ->
        decltype( declcheck( Rep::get( std::forward< Ts >( ts )... ) ) )
    {
        return Rep::get( std::forward< Ts >( ts )... );
    }

    template< typename Rep, typename... Ts >
    static std::shared_ptr< Report > _get( brick::types::NotPreferred, Ts &&... ts ) {
        return std::make_shared< Rep >( std::forward< Ts >( ts )... );
    }
};

template< typename Out >
struct TextReportBase : Report {
    Out output;

    template< typename... Ts >
    TextReportBase( Ts &&...ts ) : output( std::forward< Ts >( ts )... ) { }

    void doFinal() override {
        output << mergedReport();
    }
};

struct TextReport : TextReportBase< std::ostream & > {
    TextReport( std::ostream &o ) : TextReportBase< std::ostream & >( o ) { }
};

struct TextFileReport : TextReportBase< std::ofstream > {
    TextFileReport( std::string name ) : TextReportBase< std::ofstream >( name ) { }
};

template< typename Out >
struct PlainReportBase : Report {
    Out output;

    template< typename... Ts >
    PlainReportBase( Ts &&...ts ) : output( std::forward< Ts >( ts )... ) { }

    void doFinal() override {
        auto merged = mergedReport().report();
        for ( auto x : merged ) {
            if ( x.key != "" )
                output << x.key << ": " << x.value << std::endl;
        }
    }
};

struct PlainReport : PlainReportBase< std::ostream & > {
    PlainReport( std::ostream &o ) : PlainReportBase< std::ostream & >( o ) { }
};

struct PlainFileReport : PlainReportBase< std::ofstream > {
    PlainFileReport( std::string name ) : PlainReportBase< std::ofstream >( name ) { }
};

struct SqlReport : Report {
    SqlReport( const std::string &db, const std::string &connstr );
    void doFinal() override;

#if !OPT_SQL
    template< typename... X >
    static std::shared_ptr< Report > get( X &&...  ) { return nullptr; }
#endif

  private:
    std::string _connstr;
    std::string _db;
};

struct AggregateReport : Report {

    void signal( int s ) override {
        for ( auto r : _reports )
            r->signal( s );
    }

    void finished() override {
        for ( auto r : _reports )
            r->finished();
    }

    void execCommand( const std::string &ec ) override {
        for ( auto r : _reports )
            r->execCommand( ec );
    }

    void doFinal() override {
        for ( auto r : _reports )
            r->doFinal();
    }

    void addReport( std::shared_ptr< Report > rep ) {
        _reports.push_back( rep );
    }

  private:
    std::vector< std::shared_ptr< Report > > _reports;
};

struct NoReport : Report {
    void doFinal() override { }
    void signal( int ) override { }
    void finished() override { }
};

}

#endif

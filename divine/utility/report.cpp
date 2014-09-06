// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
#include <divine/utility/report.h>
#include <wibble/param.h>
#include <wibble/string.h>
#include <map>

#if OPT_SQL
#include <external/nanodbc/nanodbc.h>
#endif

namespace divine {
std::vector< ReportLine > Report::Merged::report() const {
    ReportLine ec{ "Execution-Command", r._execCommand };
    ReportLine empty{ "", "" };
    std::vector< ReportLine > term{
            { "Termination-Signal", std::to_string( r._signal ) },
            { "Finished", r._finished ? "Yes" : "No" }
        };

    return WithReport::merge( ec, empty, BuildInfo(), empty, r._sysinfo,
            empty, m, empty, term );
}

std::string Report::mangle( std::string str ) {
    for ( auto &x : str ) {
        x = std::tolower( x );
        if ( !std::isalnum( x ) )
            x = '_';
    }
    return str;
}

#if !OPT_SQL
SqlReport::SqlReport( const std::string &, const std::string & ) {
    wibble::exception::Consistency( "ODBC support must be included for SQL reports" );
}


void SqlReport::doFinal( const Meta & ) {
    wibble::exception::Consistency( "ODBC support must be included for SQL reports" );
}
#else

SqlReport::SqlReport( const std::string &db, const std::string &connstr ) :
    _connstr( connstr ), _db( db )
{
    // test connection string
    nanodbc::connection connection( _connstr );
    wibble::param::discard( connection );
}

void SqlReport::doFinal( const Meta &meta ) {
    nanodbc::connection connection( _connstr );
    auto merged = mergedReport( meta ).report();
    std::vector< std::string > allcols;
    std::map< std::string, std::string > values;
    for ( auto a : merged ) {
        if ( a.key != "" ) {
            values[ Report::mangle( a.key ) ] = a.value;
            allcols.push_back( Report::mangle( a.key ) );
        }
    }

    std::vector< std::string > usedkeys;

    try {
        auto res = execute( connection, "SELECT * FROM " + _db + " WHERE 1 = 0;" );

        for ( int i = 0; i < res.columns(); ++i ) {
            auto colname = res.column_name( i );
            if ( values.count( colname ) )
                usedkeys.push_back( colname );
        }
    } catch ( const nanodbc::database_error & ) {
        std::cerr << "INFO: Table not found, creating..." << std::endl;

        // create table
        usedkeys = allcols;
        std::vector< std::string > cols;
        std::transform( allcols.begin(), allcols.end(), std::back_inserter( cols ),
            []( std::string col ) { return col + " varchar"; } );
        std::stringstream create;
        create << "CREATE TABLE "
               << _db << " (" << wibble::str::fmt_container( cols, ' ', ' ' )
               << ");";
        // std::cerr << create.str() << std::endl;
        execute( connection, create.str() );
    }

    std::vector< std::string > placeholders( usedkeys.size(), "?" );
    std::stringstream com;
    com << "INSERT INTO " << _db
        << " (" << wibble::str::fmt_container( usedkeys, ' ', ' ' ) << ") "
        << "VALUES (" << wibble::str::fmt_container( placeholders, ' ', ' ' ) << ");";
    try {
        nanodbc::statement stmtins( connection );
        nanodbc::prepare( stmtins, com.str() );
        for ( int i = 0; i < int( usedkeys.size() ); ++i )
            stmtins.bind_parameter( i, values[ usedkeys[ i ] ].c_str() );
        execute( stmtins );
    } catch ( const nanodbc::database_error & ) {
        std::cerr << "There was an error inserting report to database" << std::endl;
        throw;
    }
}
#endif


}

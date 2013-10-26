#include <divine/utility/report.h>

namespace divine {
std::vector< ReportPair > Report::Merged::report() const {
    ReportPair ec{ "Execution-Command", r._execCommand };
    ReportPair empty{ "", "" };
    std::vector< ReportPair > term{
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

std::string Report::quote( std::string str ) {
    str.insert( str.begin(), '"' );
    str.insert( str.end(), '"' );
    return str;
}
}

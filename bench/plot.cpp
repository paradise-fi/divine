// -*- C++ -*- (c) 2014 Vladimír Štill <xstill@fi.muni.cz>

// please compile with -std=c++1y

/*
 * compilation: clang++ -std=c++1y bench/plot.cpp -Ibricks -o plot
 * usage example: ./plot <result directory> "Threads" "Wall-Time" "Algorithm" | gnutplot - > plot.pdf
 */

#include <sys/types.h>
#include <dirent.h>

#include <vector>
#include <string>
#include <fstream>
#include <streambuf>

#include <brick-query.h>
#include <brick-benchmark.h>

using namespace brick;

struct File {
    File() = default;
    File( std::string f, std::string c ) : filename( f ), content( c ) { }

    std::string filename;
    std::string content;
};

File readFile( std::string path ) {
    std::ifstream file{ path };
    std::string str{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
    return File( path, std::move( str ) );
}

auto getFiles( std::string path ) {
    std::vector< std::string > names;
    std::shared_ptr< DIR > dir{ opendir( path.c_str() ), []( DIR *ptr ) { closedir( ptr ); } };
    for ( struct dirent *dp = readdir( dir.get() ); dp; dp = readdir( dir.get() ) ) {
        if ( dp->d_name != std::string( "." ) && dp->d_name != std::string( ".." ) )
            names.push_back( path + "/" + dp->d_name );
    }
    return query::owningQuery( std::move( names ) ).map( readFile );
}

bool endsWith( std::string what, std::string ending ) {
    return what.size() >= ending.size()
        && what.substr( what.size() - ending.size() ) == ending;
}

std::map< std::string, std::string > toMap( const std::string &str ) {
    std::istringstream ss{ str };
    std::string line;
    std::map< std::string, std::string > map;
    while ( std::getline( ss, line ) ) {
        int colon = line.find( ':' );
        if ( colon == std::string::npos )
            continue;
        map[ line.substr( 0, colon ) ] = line.substr( colon + 2 );
    }
    return map;
};

struct Point {
    Point() : x( 0 ), y( 0 ) { }
    Point( double x, double y ) : x( x ), y( y ) { }
    double x;
    double y;
};

struct AggregatedPoint {
    AggregatedPoint() : x( 0 ) { }
    AggregatedPoint( double x, std::vector< double > y ) : x( x ), y( y ) { }

    double x;
    std::vector< double > y;
};

struct PointWithErrBar : Point {
    PointWithErrBar() : y_low( 0 ), y_high( 0 ) { }
    PointWithErrBar( double x, double y, double y_low, double y_high ) :
        Point( x, y ), y_low( y_low ), y_high( y_high )
    { }

    double y_low;
    double y_high;
};

struct DataPoint : Point {
    DataPoint() = default;
    DataPoint( std::string model, std::string label, std::string x, std::string y ) :
        Point( std::stod( x ), std::stod( y ) ), model( model ), label( label )
    { }

    std::string model;
    std::string label;
    double x;
    double y;
};

template< typename Pt = Point >
struct Line {
    Line() = default;
    Line( std::string label, std::vector< Pt > line ) :
        label( label ), line( line )
    { }

    std::string label;
    std::vector< Pt > line;
};

template< typename Pt = Point >
struct Result {
    Result() = default;
    Result( std::string model, std::vector< Line< Pt > > lines ) :
        model( model ), lines( lines )
    { }

    std::string model;
    std::vector< Line< Pt > > lines;
};

struct ProcessedResult : Result< PointWithErrBar > {
    using Result< PointWithErrBar >::Result;
};

struct BenchmarkDesc {
    BenchmarkDesc( std::string model, std::string label, double x ) :
        model( model ), label( label ), xParam( x )
    { }
    std::string model;
    std::string label;
    double xParam;
};

struct AggregatedResult : Result< AggregatedPoint > {
    using Result< AggregatedPoint >::Result;

    // returns processed report and list of unsatisfactory tests
    std::pair< ProcessedResult, std::vector< BenchmarkDesc > > process() const {
        ProcessedResult rep{ model, { } };
        std::vector< BenchmarkDesc > unsat;
        rep.lines = query::query( lines ).map( [&]( auto line ) {
                return Line< PointWithErrBar >{
                    line.label,
                    query::query( line.line )
                        .filter( []( auto point ) { return point.y.size(); } )
                        .map( [&]( auto point ) {
#if BOOTSTRAP
                            benchmark::SampleStats stats;
                            stats.sample = point.y;
                             if ( stats.processSamples( 50, -1 ) == benchmark::SampleStats::Unsatisfactory )
                                unsat.emplace_back( model, line.label, point.x );
                            return PointWithErrBar{ point.x, stats.m_mean, stats.b_mean.low, stats.b_mean.high };
#else
                            auto sorted = query::query( point.y ).sort().freeze();
                            auto h = sorted.size() / 2;
                            auto q = sorted.size() / 4;
                            auto qq = sorted.size() - q;
                            if ( qq == sorted.size() )
                                --qq;
                            return PointWithErrBar{ point.x, sorted[ h ], sorted[ q ], sorted[ qq ] };
#endif
                        } ).freeze()
                };
            } ).freeze();
        return std::make_pair( rep, unsat );
    }
};

struct RawResult : Result< Point > {
    using Result< Point >::Result;

    AggregatedResult aggregate() const {
        auto val = AggregatedResult{
            model,
            query::query( lines ).map( []( const auto &l ) {
                    return Line< AggregatedPoint >{
                        l.label,
                        query::query( l.line )
                            .groupBy( []( Point p ) { return p.x; } )
                            .map( []( auto pair ) {
                                    auto ys = query::query( pair.second )
                                            .map( []( Point p ) { return p.y; } )
                                            .freeze();
                                    return AggregatedPoint{ pair.first, ys };
                                } )
                            .freeze()
                    };
                } )
            .freeze()
        };
        return val;
    }
};


Result< PointWithErrBar > aggregateResults( const Result<> &src ) {
    auto lines = query::query( src.lines ).map( []( const Line<> &l ) {
            auto aggregate = query::query( l.line )
                .groupBy( []( Point p ) { return p.x; } )
                .map( []( auto p ) {
                        auto q = query::query( p.second ).map( []( Point p ) { return p.y; } );
                        return PointWithErrBar{ p.first, q.median(), q.min(), q.max() };
                    } )
                .freeze();
            return Line< PointWithErrBar >{ l.label, aggregate };
        } ).freeze();;
    return Result< PointWithErrBar >{ src.model, lines };
}

[[noreturn]] void die( std::string msg ) {
    std::cerr << "FATAL: " << msg << std::endl;
    std::abort();
}

template< typename Pt >
void dump( const std::vector< Result< Pt > > &results ) {
    for ( const auto &res : results ) {
        std::cerr << res.model << std::endl;
        for ( const auto &l : res.lines ) {
            std::cerr << " -> " << l.label << std::endl;
            for ( const auto p : l.line )
                std::cerr << "        ( " << p.x << ", " << p.y << " )" << std::endl;
        }
        std::cerr << std::endl;
    }
}

int main( int argc, char **argv ) {
    if ( argc <= 4 )
        die( "usage: <result dir> <x-axe parameter> <y-axe parameter> <dateset parameter>" );
    std::string xAxe{ argv[ 2 ] };
    std::string yAxe{ argv[ 3 ] };
    std::string dataset{ argv[ 4 ] };

    std::vector< RawResult > rawResults = getFiles( argv[ 1 ] )
        .filter( []( File f ) { return endsWith( f.filename, ".rep" ) && f.content.size(); } )
        .map( [&]( File f ) {
                auto map = toMap( f.content );
                return DataPoint( map[ "Model" ], map[ dataset ], map[ xAxe ], map[ yAxe ] );
            } )
        .groupBy( []( DataPoint &dp ) { return dp.model; } )
        .map( []( auto pair ) {
                return RawResult{ pair.first, query::query( pair.second )
                        .groupBy( []( auto dp ) { return dp.label; } )
                        .map( []( auto pair ) {
                                return Line<>{ pair.first, query::query( pair.second )
                                    .map( []( DataPoint dp ) { return Point( dp ); } )
                                    .freeze() };
                            } )
                        .freeze() };
            } )
        .freeze();

    auto resultsP = query::query( rawResults ).map( []( auto r ) { return r.aggregate().process(); } ).freeze();
    std::vector< ProcessedResult > results = query::query( resultsP ).map( []( auto p ) { return p.first; } ).freeze();
    auto unsat = query::query( resultsP ).concatMap( []( auto p ) { return p.second; } ).freeze();

    if ( unsat.size() ) {
        std::cerr << "WARNING: Following data points have unsatisfactory quality" << std::endl;
        for ( auto x : unsat )
            std::cerr << "{ Model = " << x.model
                      << ", " << dataset << " = " << x.label
                      << ", " << xAxe << " = " << x.xParam
                      << " }" << std::endl;
    }

    gnuplot::Plots plots;
    for ( auto &r : results ) {
        gnuplot::Plot &plot = plots.append();
        double xmin = r.lines[ 0 ].line[ 0 ].x,
               xmax = xmin,
               ymin = r.lines[ 0 ].line[ 0 ].y,
               ymax = ymin;
        for ( auto &l : r.lines ) {
            auto &ds = plot.append( l.label, 0, 4, gnuplot::DataSet::Box );
            for ( PointWithErrBar &p : l.line ) {
                xmax = std::max( xmax, p.x );
                xmin = std::min( xmin, p.x );
                ymax = std::max( ymax, p.y_high );
                ymin = std::min( ymin, p.y_low );
                ds.append( p.x, p.y, p.y_low, p.y_high );
            }
        }
        // plot.rescale  ( gnuplot::Plot::Y, t_mult );
        plot.bounds   ( gnuplot::Plot::X, xmin - 1 , xmax + 1 );
        plot.bounds   ( gnuplot::Plot::Y, std::min( 0.0, ymin * 1.05 ), ymax * 1.05 );
        // plot.interval ( gnuplot::Plot::X, x.log ? pow(x.step, k) : x.step * k );
        plot.axis     ( gnuplot::Plot::X, xAxe, "" );
        plot.axis     ( gnuplot::Plot::Y, yAxe, "" );
        plot.axis     ( gnuplot::Plot::Z, dataset, "" );
        plot.name     ( r.model );
        plot.style    ( gnuplot::Style::Pattern );
    }
    std::cout << plots.plot();
    return 0;
}

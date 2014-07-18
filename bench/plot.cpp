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


#include <brick-gnuplot.h>
#include <brick-query.h>
#include <brick-unittest.h>

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

std::vector< File > getFiles( std::string path ) {
    std::vector< std::string > names;
    std::shared_ptr< DIR > dir{ opendir( path.c_str() ), []( DIR *ptr ) { closedir( ptr ); } };
    for ( struct dirent *dp = readdir( dir.get() ); dp; dp = readdir( dir.get() ) )
        if ( dp->d_type == DT_REG )
            names.push_back( path + "/" + dp->d_name );
    return query::query( names ).map( readFile ).freeze();
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


Result< PointWithErrBar > aggregateResults( const Result<> &src ) {
    auto lines = query::query( src.lines ).map( []( const Line<> &l ) {
            auto aggregate = query::query( l.line )
                .groupBy( []( Point p ) { return p.x; } )
                .map( []( auto p ) {
                        auto q = query::query( p.second ).map( []( Point p ) { return p.y; } );
                        return PointWithErrBar{ p.first, q.mean(), q.min(), q.max() };
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
    auto files = getFiles( argv[ 1 ] );
    std::string xAxe{ argv[ 2 ] };
    std::string yAxe{ argv[ 3 ] };
    std::string dataset{ argv[ 4 ] };

    auto results = query::query( files )
        .filter( []( File f ) { return endsWith( f.filename, ".rep" ) && f.content.size(); } )
        .map( [&]( File f ) {
                auto map = toMap( f.content );
                return DataPoint( map[ "Model" ], map[ dataset ], map[ xAxe ], map[ yAxe ] );
            } )
        .groupBy( []( DataPoint &dp ) { return dp.model; } )
        .map( []( auto pair ) {
                return Result<>{ pair.first, query::query( pair.second )
                        .groupBy( []( auto dp ) { return dp.label; } )
                        .map( []( auto pair ) {
                                return Line<>{ pair.first, query::query( pair.second )
                                    .map( []( DataPoint dp ) { return Point( dp ); } )
                                    .freeze() };
                            } )
                        .freeze() };
            } )
        .map( aggregateResults )
        .freeze();
    dump( results );

    gnuplot::Plots plots;
    for ( auto &r : results ) {
        gnuplot::Plot &plot = plots.append();
        for ( auto &l : r.lines ) {
            auto &ds = plot.append( l.label, 0, 4, gnuplot::DataSet::RibbonLP );
            for ( PointWithErrBar &p : l.line )
                ds.append( p.x, p.y, p.y_low, p.y_high );
        }
        // plot.rescale  ( gnuplot::Plot::Y, t_mult );
        // plot.bounds   ( gnuplot::Plot::X, x.scaled( x.min ), x.scaled( x.max ) );
        // plot.interval ( gnuplot::Plot::X, x.log ? pow(x.step, k) : x.step * k );
        plot.axis     ( gnuplot::Plot::X, xAxe, "" );
        plot.axis     ( gnuplot::Plot::Y, yAxe, "" );
        plot.axis     ( gnuplot::Plot::Z, dataset, "" );
        plot.name     ( r.model );
        plot.style    ( gnuplot::Style::Gradient );
    }
    std::cout << plots.plot();
    return 0;
}

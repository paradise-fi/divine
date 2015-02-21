#include <regex>
#include <vector>
#include <iostream>
#include <fstream>

#include <brick-data.h>
#include <brick-types.h>
#include <brick-commandline.h>
#include <brick-gnuplot.h>

struct Statistics {
    using TableIt = std::vector< std::vector< int64_t > >::const_iterator;

    struct SRef {
        explicit SRef( int v ) : val( v ) { }
        int val;

        bool operator<( SRef o ) const { return val < o.val; }
    };

    struct Iterator : brick::types::Comparable {
        Iterator( const Statistics *self, TableIt row ) :
            _self( self ),
            _row( row )
        { }

        int64_t operator[]( SRef ref ) { return (*_row)[ ref.val ]; }
        int64_t operator[]( std::string key ) {
            return (*this)[ _self->ref( key ) ];
        }

        Iterator &operator++() { ++_row; return *this; }
        Iterator &operator--() { --_row; return *this; }

        bool operator==( const Iterator &o ) const { return _row == o._row; }
        bool operator<( const Iterator &o ) const { return _row < o._row; }

      private:
        const Statistics *_self;
        TableIt _row;
    };

    explicit Statistics( std::istream &s ) {
        const std::regex valid {
            "^[^ ].*$", std::regex_constants::egrep
                      | std::regex_constants::optimize };
        const std::regex keyval{
            "([^=; ]*)=([^=; ]*);", std::regex_constants::egrep
                                  | std::regex_constants::optimize };
        const std::sregex_iterator srend;

        std::string line;
        auto bump = [&]() -> bool {
            do {
                if ( !std::getline( s, line ) )
                    return false;
            } while ( !std::regex_match( line.begin(), line.end(), valid ) );
            return true;
        };

        if ( !bump() )
            return;

        if ( line.substr( 0, 6 ) == std::string( "meta: " ) ) {
            auto meta = line.substr( 6 );
            std::sregex_iterator it( meta.begin(), meta.end(), keyval );
            for ( ; it != srend; ++it )
                _meta.emplace_back( (*it)[ 1 ], (*it)[ 2 ] );
            if ( !bump() )
                return;
        }

        std::sregex_iterator it( line.begin(), line.end(), keyval );
        for ( int i = 0; it != srend; ++it, ++i )
            _keys.insert( (*it)[ 1 ], SRef( i ) );

        do {
            _data.emplace_back();
            auto &row = _data.back();
            it = std::sregex_iterator( line.begin(), line.end(), keyval );
            for ( int i = 0; it != srend; ++it, ++i )
            {
                ASSERT_EQ( (*it)[ 1 ], _keys[ SRef( i ) ] );
                row.emplace_back( std::stoll( (*it)[ 2 ] ) );
            }
        } while ( bump() );
    }

    SRef ref( std::string s ) const { return _keys[ s ]; }

    Iterator begin() const { return Iterator( this, _data.begin() ); }
    Iterator end() const { return Iterator( this, _data.end() ); }

    struct Proxy {
        Proxy( const Statistics *stat, std::string key, std::string value ) :
            _self( stat ), _key( stat->ref( key ) ), _val( stat->ref( value ) )
        { }

        struct PIterator : brick::types::Comparable {
            PIterator( const Proxy *proxy, Iterator row ) :
                _proxy( proxy ), _row( row )
            { }

            std::pair< int64_t, int64_t > operator*() { return _get(); }
            std::pair< int64_t, int64_t > *operator->() { return &_get(); }

            PIterator &operator++() { ++_row; return *this; }
            PIterator &operator--() { --_row; return *this; }

            bool operator==( const PIterator &o ) const { return _row == o._row; }
            bool operator<( const PIterator &o ) const { return _row < o._row; }

          private:
            const Proxy *_proxy;
            Iterator _row;

            std::pair< int64_t, int64_t > _cache;
            std::pair< int64_t, int64_t > &_get() {
                _cache = { _row[ _proxy->_key ], _row[ _proxy->_val ] };
                return _cache;
            }
        };

        PIterator begin() const { return PIterator( this, _self->begin() ); }
        PIterator end() const { return PIterator( this, _self->end() ); }

      private:
        const Statistics *_self;
        SRef _key;
        SRef _val;
    };

    Proxy proxy( std::string key, std::string value ) const {
        return Proxy( this, key, value );
    }

    std::string label( std::vector< std::string > keys ) const {
        std::stringstream ss;
        for ( const auto &k : keys )
            for ( const auto &p : _meta )
                if ( k == p.first )
                    ss << p.second << "-";
        auto s = ss.str();
        return s.substr( 0, s.size() - 1 );
    }

  private:
    brick::data::Bimap< std::string, SRef > _keys;
    std::vector< std::vector< int64_t > > _data;
    std::vector< std::pair< std::string, std::string > > _meta;
};

int main( int argc, char **argv ) {
    namespace cmd = brick::commandline;
    namespace gnuplot = brick::gnuplot;

    cmd::StandardParser parser( "stat", "0.1" );
    auto gplot = parser.createGroup( "Plotting options" );

    auto key = gplot->add< cmd::StringOption >(
            "key", 'k', "key", "",
            "Key value name, that is value on x-axis" );
    auto val = gplot->add< cmd::StringOption >(
            "value", 'v', "value", "",
            "Value name, that is value on y-axis" );
    auto labelKey = gplot->add< cmd::StringOption >(
            "label-key", '\0', "label-key", "",
            "Label for x-axis" );
    auto labelVal = gplot->add< cmd::StringOption >(
            "label-value", '\0', "label-value", "",
            "Label for y-axis" );
    auto labelLegend = gplot->add< cmd::StringOption >(
            "label-legent", '\0', "label-legend", "",
            "Label of lengend" );
    auto name = gplot->add< cmd::StringOption >(
            "name", 'n', "name", "",
            "Name of graph" );
    auto legend = gplot->add< cmd::VectorOption< cmd::String > >(
            "legend", 'L', "legend", "",
            "meta values in legend for plot identification" );
    auto xlog = gplot->add< cmd::BoolOption >(
            "x-log", '\0', "x-log", "",
            "Use logarithmic x-axis" );
    auto ylog = gplot->add< cmd::BoolOption >(
            "y-log", '\0', "y-log", "",
            "Use logarithmic y-axis" );
    auto xinterval = gplot->add< cmd::IntOption >(
            "x-interval", '\0', "x-interval", ""
            "Interval between values on x-axis" );
    auto yinterval = gplot->add< cmd::IntOption >(
            "y-interval", '\0', "y-interval", ""
            "Interval between values on y-axis" );
    auto noyfix = gplot->add< cmd::BoolOption >(
            "no-y-fix", '\0', "no-y-fix", ""
            "Do not fix minimal value of y-ais to 0" );
    auto xminopt = gplot->add< cmd::IntOption >(
            "x-min-bound", 0, "x-min-bound", "",
            "Minimal value on x-axis" );
    auto xmaxopt = gplot->add< cmd::IntOption >(
            "x-max-bound", 0, "x-max-bound", "",
            "Maximal value on x-axis" );
    auto yminopt = gplot->add< cmd::IntOption >(
            "y-min-bound", 0, "y-min-bound", "",
            "Minimal value on y-axis" );
    auto ymaxopt = gplot->add< cmd::IntOption >(
            "y-max-bound", 0, "y-max-bound", "",
            "Maximal value on y-axis" );

    parser.add( gplot );
    parser.setPartialMatchingRecursively( true );

    try {
        parser.parse( argc, const_cast< const char ** >( argv ) );
    } catch ( cmd::BadOption &ex ) {
        std::cerr << "FATAL: " << ex.what() << std::endl;
        exit( 1 );
    }
    if ( parser.help->boolValue() || parser.version->boolValue() )
        return 0;

    if ( !key->boolValue() ) {
        std::cerr << "FATAL: --key | -k must be set" << std::endl;
        exit( 1 );
    }

    if ( !val->boolValue() ) {
        std::cerr << "FATAL: --value | -v must be set" << std::endl;
        exit( 1 );
    }

    std::vector< std::pair< std::string, Statistics > > stat;
    for ( std::string in; in = parser.next(), !in.empty(); ) {
        std::cerr << "Parsing " << in << "... " << std::flush;
        std::ifstream inf( in );
        stat.emplace_back( std::piecewise_construct,
                std::make_tuple( in ), std::make_tuple( std::ref( inf ) ) );
        std::cerr << "done" << std::endl;
    }

    gnuplot::Plots plots;
    gnuplot::Plot &plot = plots.append();

    bool set = false;
    int64_t xmin, xmax = xmin, ymin, ymax;

    for ( auto &p : stat ) {
        auto file = p.first;
        auto s = p.second;
        auto data = s.proxy( key->stringValue(), val->stringValue() );
        if ( !set && data.begin() != data.end() ) {
            set = true;
            xmin = xmax = data.begin()->first;
            ymin = ymax = data.begin()->second;
        }

        auto label = legend->boolValue() ? s.label( legend->values() ) : file;
        auto &ds = plot.append( label, 0, 2, gnuplot::DataSet::Line, false );
        for ( auto p : data ) {
            xmax = std::max( xmax, p.first );
            xmin = std::min( xmin, p.first );
            ymax = std::max( ymax, p.second );
            ymin = std::min( ymin, p.second );
            ds.append( double( p.first ), double( p.second ) );
        }
    }

    auto xAxis = labelKey->boolValue() ? labelKey->stringValue() : key->stringValue();
    auto yAxis = labelVal->boolValue() ? labelVal->stringValue() : val->stringValue();
    auto label = name->boolValue() ? name->stringValue() : xAxis + " vs. " + yAxis;

    auto xminb = xminopt->boolValue() ? xminopt->intValue() : xmin - 1;
    auto xmaxb = xmaxopt->boolValue() ? xmaxopt->intValue() : xmax - 1;
    plot.bounds( gnuplot::Plot::X, xminb, xmaxb );

    auto yminb = yminopt->boolValue() ? yminopt->intValue() : ymin * 0.95;
    if ( !noyfix->boolValue() && !ylog->boolValue() && !yminopt->boolValue() )
        yminb = std::min( 0.0, yminb );
    auto ymaxb = ymaxopt->boolValue() ? ymaxopt->intValue() : ymax * 1.05;
    plot.bounds( gnuplot::Plot::Y, yminb, ymaxb );

    plot.axis( gnuplot::Plot::X, xAxis, "" );
    plot.axis( gnuplot::Plot::Y, yAxis, "" );
    plot.axis( gnuplot::Plot::Z, labelLegend->stringValue(), "" );

    if ( xlog->boolValue() )
        plot.logscale( gnuplot::Plot::X );
    if ( ylog->boolValue() )
        plot.logscale( gnuplot::Plot::Y );
    if ( xinterval->boolValue() )
        plot.interval( gnuplot::Plot::X, xinterval->intValue() );
    if ( yinterval->boolValue() )
        plot.interval( gnuplot::Plot::Y, yinterval->intValue() );

    plot.name( label );
    plot.style( gnuplot::Style::Gradient );

    std::cout << plots.plot();
}

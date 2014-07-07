// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * Write benchmarks for C++ units.
 */

/*
 * (c) 2014 Petr Ročkai <me@mornfall.net>
 */

/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#include <brick-unittest.h>

#include <numeric>
#include <cmath>
#include <algorithm>

#include <time.h>

#ifdef __unix
#include <sys/types.h>
#include <sys/socket.h>
#endif

#ifndef BRICK_BENCHMARK_H
#define BRICK_BENCHMARK_H

namespace brick {
namespace benchmark {

using Sample = std::vector< double >;

struct Matrix {
    std::vector< double > m;
    int width;

    int index( int r, int c ) {
        ASSERT_LEQ( r * width + c, int( m.size() ) - 1 );
        return r * width + c;
    }

    int height() { return m.size() / width; }

    struct Proxy {
        Matrix &A;
        int x;
        double &operator[]( double y ) {
            return A.m[ A.index( x, y ) ];
        }
        Proxy( Matrix &A, int x ) : A( A ), x( x ) {}
    };

    Proxy operator[]( double x ) {
        return Proxy( *this, x );
    }

    Matrix( int height, int width )
        : m( width * height, 0.0 ), width( width )
    {}

    std::vector< double > solve() {
        Matrix &A = *this;
        int m = A.height();

        for( int k = 0; k < m; ++ k )
        {
            int i_max = 0; // pivot for column
            double vali = -std::numeric_limits< double >::infinity();

            for ( int i = k; i < m; ++ i )
                if( A[i][k] > vali ) { i_max = i; vali = A[i][k]; }

            A.swapRows( k, i_max );

            for( int i = k + 1; i < m; ++ i ) // for all rows below pivot
            {
                for(int j = k + 1; j < m + 1; ++ j )
                    A[i][j] = A[i][j] - A[k][j] * (A[i][k] / A[k][k]);
                A[i][k] = 0;
            }
        }

        std::vector< double > x( m, 0.0 );

        for( int i = m - 1; i >= 0; -- i) // rows = columns
        {
            double v = A[i][m] / A[i][i];
            x[i] = v;
            for( int j = i - 1; j >= 0; -- j) // rows
            {
                A[j][m] -= A[j][i] * v;
                A[j][i] = 0;
            }
        }

        return x;
    }

    void swapRows( int k, int l ) {
        Matrix &A = *this;
        for ( int i = 0; i < width; ++i )
            std::swap( A[k][i], A[l][i] );
    }
};

struct Spline {
    std::vector< double > xs, ys, ks;

    void push( double x, double y ) {
        xs.push_back( x );
        ys.push_back( y );
    }

    void interpolateNaturalKs() {
        int n = xs.size() - 1;
        Matrix A( n + 1, n + 2 );

        for( int i = 1; i < n; ++ i ) // rows
        {
            A[i][i-1] = 1 / (xs[i] - xs[i-1]);
            A[i][i  ] = 2 * (1/(xs[i] - xs[i-1]) + 1/(xs[i+1] - xs[i])) ;
            A[i][i+1] = 1 / (xs[i+1] - xs[i]);
            A[i][n+1] = 3 * ( (ys[i]-ys[i-1]) / ((xs[i] - xs[i-1]) * (xs[i] - xs[i-1]))  +
                              (ys[i+1]-ys[i]) / ((xs[i+1] - xs[i]) * (xs[i+1] - xs[i])) );
        }

        A[0][  0] = 2/(xs[1] - xs[0]);
        A[0][  1] = 1/(xs[1] - xs[0]);
        A[0][n+1] = 3 * (ys[1] - ys[0]) / ((xs[1]-xs[0])*(xs[1]-xs[0]));

        A[n][n-1] = 1 / (xs[n] - xs[n-1]);
        A[n][  n] = 2 / (xs[n] - xs[n-1]);
        A[n][n+1] = 3 * (ys[n] - ys[n-1]) / ((xs[n]-xs[n-1])*(xs[n]-xs[n-1]));

        ks = A.solve();
    }

    double eval( double x ) {
        int i = 1;
        while(xs[i]<x) i++;

        double t = (x - xs[i-1]) / (xs[i] - xs[i-1]);
        double a =  ks[i-1]*(xs[i]-xs[i-1]) - (ys[i]-ys[i-1]);
        double b = -ks[i  ]*(xs[i]-xs[i-1]) + (ys[i]-ys[i-1]);
        double q = (1-t)*ys[i-1] + t*ys[i] + t*(1-t)*(a*(1-t)+b*t);

        return q;
    }
};

struct Estimate {
    double low, high, mean;
};

struct Box {
    double low, high, median, width;
};

Box box( Sample s, double width = 0.5 )
{
    std::sort( s.begin(), s.end() );
    Box result;
    double wing = (1 - width) / 2;
    result.low = s[ int( floor( s.size() * wing ) ) ];
    result.median = s[ s.size() / 2 ];
    result.high = s[ int( ceil( s.size() * (1 - wing) ) ) ];
    return result;
}

double sum( Sample s ) {
    double r = 0;
    for ( auto n : s )
        r += n;
    return r;
}

double mean( Sample s ) {
    return sum( s ) / s.size();
}

double stddev( Sample s ) {
    double avg = mean( s ), sum = 0;
    for ( auto n : s )
        sum += (avg - n) * (avg - n);
    return sum / s.size();
}

template< typename E >
Sample bootstrap( Sample s, E estimator, int iterations = 20000 )
{
    struct random_data rand;
    char statebuf[256];
    memset( &rand, 0, sizeof(rand) );
    initstate_r( 0, statebuf, sizeof( statebuf ), &rand );

    Sample result;
    for ( int i = 0; i < iterations; ++i ) {
        Sample resample;
        while ( resample.size() < s.size() ) {
            int idx;
            random_r( &rand, &idx );
            resample.push_back( s[ idx % s.size() ] );
        }
        result.push_back( estimator( resample ) );
    }
    return result;
}

struct Axis {
    bool active; /* only evaluate the axis if it is active */
    bool log; /* if true, step is multiplicative, in percent */
    enum { Mult, Div, None } normalize; /* scale time per unit on this axis? */
    int64_t min, max;
    double step; // useful for log-scaled benchmarks
    double unit_mul, unit_div;
    std::string name, unit;

    std::function<std::string(int64_t)> _render;

    Axis() : active( false ), log( false ), normalize( None ),
             min( 1 ), max( 10 ), step( 1 ),
             unit_mul( 1 ), unit_div( 1 ),
             name( "n" ), unit( "unit" )
    {
        _render = []( int64_t ) { return ""; };
    }

    std::string render( int64_t p ) {
        if ( !_render( p ).empty() )
            return _render( p );
        std::stringstream s;
        s << int( round( scaled( p ) ) );
        return s.str();
    }

    double scaled( double p ) { return (p * unit_mul) / unit_div; }
    double normal( double p ) {
        switch( normalize ) {
            case None: return 1;
            case Mult: return p;
            case Div: return 1.0 / p;
        }
    }

    int amplitude() {
        return floor( 1 + log10( scaled( max ) ) );
    }

    int count() {
        if ( !active )
            return 1;
        ASSERT_LEQ( min, max );
        if ( log ) { // use floating point ::log?
            int r = 1, n = min;
            while ( n < max ) {
                ++ r;
                n = n * step;
            }
            return r;
        }
        return 1 + (max - min) / step;
    }
};

struct BenchmarkBase : unittest::TestCaseBase {
    struct timespec start, end;
    int fds[2];

    virtual double normal() = 0;
    virtual int parameter( Axis, int ) = 0;
    virtual std::pair< Axis, Axis > axes() = 0;
    int64_t p, q;
};


struct BenchmarkGroup {
    Axis x, y; // z is time
    int64_t p, q; // parameter values on x and y axes
    BenchmarkGroup() : p( 0 ), q( 0 ) {}
    virtual int64_t parameter( Axis a, int seq ) {
        if ( !a.log )
            return a.min + seq * a.step;
        for ( int i = 0; i < seq; ++i )
            a.min = a.min * a.step;
        return a.min;
    }
    virtual void setup( int _p, int _q ) { p = _p; q = _q; }
    virtual std::string describe() { return ""; }
    virtual std::string describe_axes() {
        return "x=" + x.name + " y=" + y.name +
               " x_unit=" + x.unit + " y_unit=" + y.unit;
    }
    virtual double normal() { return 1.0; }
};

namespace {

std::vector< BenchmarkBase * > *benchmarks = nullptr;

std::vector< std::string > time_units = { "s ", "ms", "μs", "ns", "ps", "fs", "as", "zs", "ys" };

std::string render_ci( double point, double low_err, double high_err, double factor = 1 )
{
    int scale = 0;
    double mult = factor;

    std::stringstream str;

    while ( point * mult < 1 &&
            low_err * mult < 0.01 &&
            high_err * mult < 0.01 &&
            scale < 8 ) {
        ++ scale;
        mult = factor * pow( 1000, scale );
    }

    str << std::fixed << std::setprecision( 2 ) << "(∓"
        << std::setw( 4 ) << low_err * mult << " "
        << std::setw( 6 ) << point * mult << " " << time_units[ scale ] << " ±"
        << std::setw( 4 ) << high_err * mult << ")";
    return str.str();
}

struct ResultSet {
    Spline mean, mean_low, mean_high;

    void push( int x, double m, double l, double h ) {
        mean.push( x, m );
        mean_low.push( x, l );
        mean_high.push( x, h );
    }

    void interpolate() {
        mean.interpolateNaturalKs();
        mean_low.interpolateNaturalKs();
        mean_high.interpolateNaturalKs();
    }

    std::string sample( double x, double mult ) {
        std::stringstream str;
        str << mean.eval( x ) * mult << " "
            << mean_low.eval( x ) * mult << " "
            << mean_high.eval( x )* mult ;
        return str.str();
    }
};

void repeat( BenchmarkBase *tc, ResultSet &res ) {
#ifdef __unix
    char buf[1024];
    ::socketpair( AF_UNIX, SOCK_STREAM, PF_UNIX, tc->fds );
#endif
    Sample sample;

    Sample bs_median, bs_mean, bs_stddev;
    Box b_sample, b_median, b_mean, b_stddev;
    double m_sample, m_mean, m_median;
    double sd_sample;

    Axis x = tc->axes().first, y = tc->axes().second;

    int iterations = 0;

    while ( ( sum( sample ) < 3 && iterations < 300 ) ||
            ( sample.size() < 100 && iterations < 200 ) ) {
        for ( int i = 0; i < 10; ++i ) { /* get 10 data points at once */
            iterations ++;
            unittest::fork_test( tc, tc->fds );
#ifdef __unix
            int r = ::read( tc->fds[0], buf, sizeof( buf ) );
            if ( r >= 0 )
                buf[r] = 0;
            std::stringstream parse( buf );
            double time;
            parse >> time;
            sample.push_back( time );
#endif
        }

        b_sample = box( sample );
        m_sample = mean( sample );
        sd_sample = stddev( sample );

        double iqr = b_sample.high - b_sample.low;

        bs_mean = bootstrap( sample, mean );
        bs_median = bootstrap( sample, []( Sample s ) { return box( s ).median; } );
        bs_stddev = bootstrap( sample, stddev );

        b_mean = box( bs_mean, 0.95 );
        b_median = box( bs_median, 0.95 );
        b_stddev = box( bs_stddev, 0.95 );

        m_mean = mean( bs_mean );

        if ( b_median.high - b_median.low < 0.05 * b_sample.median &&
             b_mean.high - b_mean.low < 0.05 * m_sample &&
             ( sum( sample ) > 1 || sample.size() >= 100 ) )
            break; /* the confidence interval is less than 5% => good enough */

        /* if the sample is reasonably big but unsatisfactory, cut off outliers */
        if ( sample.size() > 50 || sum( sample ) > 5 ) {
            auto end = std::remove_if( sample.begin(), sample.end(),
                                       [&]( double n ) { return fabs(n - m_sample) > 3 * iqr; } );
            sample.erase( end, sample.end() ); // TODO: store the outliers
        }
    }

    double factor = x.normal( tc->p ) * y.normal( tc->q ) * tc->normal();

    std::cerr << "  " << x.name << ": " << std::setw( x.amplitude() ) << x.render( tc->p ) << " " << x.unit
              << " "  << y.name << ": " << std::setw( y.amplitude() ) << y.render( tc->q ) << " " << y.unit
              << " μ: " << render_ci( m_mean, m_mean - b_mean.low, b_mean.high - m_mean, factor )
              << " m: " << render_ci( b_sample.median,
                                      b_sample.median - b_median.low,
                                      b_median.high - b_sample.median, factor )
              << " σ: " << render_ci( sd_sample,
                                      sd_sample - b_stddev.low,
                                      b_stddev.high - sd_sample, factor )
              << " | n = " << std::setw( 3 ) << sample.size()
              << ", bad = " << std::setw( 3 ) << iterations - sample.size() << std::endl;

    res.push( tc->p, m_mean * factor, b_mean.low * factor, b_mean.high * factor );

    ::close( tc->fds[0] );
    ::close( tc->fds[1] );
}

struct Filter {
    using Clause = std::vector< std::string >;
    using F = std::vector< Clause >;

    F formula;

    bool matches( std::string d ) {
        for ( auto clause : formula ) {
            bool ok = false;
            for ( auto atom : clause )
                if ( d.find( atom ) != std::string::npos ) {
                    ok = true;
                    break;
                }
            if ( !ok )
                return false;
        }
        return true;
    }

    /* TODO duplicated from brick-shelltest */
    template< typename C >
    void split( std::string s, C &c ) {
        std::stringstream ss( s );
        std::string item;
        while ( std::getline( ss, item, ',' ) )
            c.push_back( item );
    }

    Filter( int argc, const char **argv ) {
        for ( int i = 1; i < argc; ++i ) {
            formula.emplace_back();
            split( argv[i], formula.back() );
        }
    }
};

void run( int argc, const char **argv ) {
    ASSERT( benchmarks );
    std::set< std::string > done;
    Filter flt( argc, argv );

    std::cout << "set terminal pdfcairo enhanced font 'Liberation Sans,10'" << std::endl;

    /* set up line styles */
    std::cout << "set style line 1 lc rgb '#ff4500' lt 1 lw 2" << std::endl
              << "set style line 2 lc rgb '#ffa500' lt 1 lw 2" << std::endl
              << "set style line 3 lc rgb '#006400' lt 1 lw 2" << std::endl
              << "set style line 4 lc rgb '#0000ff' lt 1 lw 2" << std::endl
              << "set style line 5 lc rgb '#9400d3' lt 1 lw 2" << std::endl
              << "set style line 6 lc rgb '#800000' lt 1 lw 2" << std::endl
              << "set style line 7 lc rgb '#ff0000' lt 1 lw 2" << std::endl
              << "set style fill transparent solid 0.3" << std::endl;

    /* set up axis styles */
    std::cout << "set style line 11 lc rgb '#808080' lt 1" << std::endl
              << "set border 3 back ls 11" << std::endl
              << "set tics nomirror out scale 0.75" << std::endl
              << "set style line 12 lc rgb'#808080' lt 0 lw 1" << std::endl
              << "set grid back ls 12" << std::endl
              << "set arrow from graph 1,0 to graph 1.05,0 size screen 0.025,10,60 filled ls 11" << std::endl
              << "set arrow from graph 0,1 to graph 0,1.05 size screen 0.025,10,60 filled ls 11" << std::endl;

    for ( auto group : *benchmarks ) {
        if ( done.count( group->group() ) )
            continue;
        for ( auto tc : *benchmarks ) {
            if ( group->group() != tc->group() )
                continue;

            std::string t_desc = tc->group() + " test=" + tc->name;
            if ( !flt.matches( t_desc ) )
                continue;

            std::cerr << "## " << t_desc << std::endl;
            auto axes = tc->axes();
            Axis x = axes.first, y = axes.second;

            std::vector< ResultSet > results;

            for ( int q_seq = 0; q_seq < y.count(); ++ q_seq ) {
                ResultSet res;
                for ( int p_seq = 0; p_seq < x.count(); ++ p_seq ) {
                    tc->p = tc->parameter( x, p_seq );
                    tc->q = tc->parameter( y, q_seq );
                    repeat( tc, res );
                }
                results.push_back( res );
            }

            double t_max = 0, t_mult = 1;
            int t_scale = 0;

            std::stringstream header;

            header << "plot \\" << std::endl;
            for ( int q_seq = 0; q_seq < y.count(); ++ q_seq ) {
                for ( int p_seq = 0; p_seq < x.count(); ++ p_seq )
                    t_max = std::max( t_max, results[ q_seq ].mean_high.ys[ p_seq ] );

                header << " '-' using 1:3:4 title \"" << y.render( tc->parameter( y, q_seq ) )
                       << "\" with filledcurves ls " << q_seq + 1;
                header << ", '-' using 1:2 notitle with lines ls " << q_seq + 1;
                header << ", '-' using 1:3 notitle with lines ls " << q_seq + 1 << " lw 0.5";
                header << ", '-' using 1:4 notitle with lines ls " << q_seq + 1 << " lw 0.5";
                header << ", '-' using 1:2 notitle with points ls " << q_seq + 1 << " pt 7 ps 0.1 lc rgb '#000000'";
                if ( q_seq + 1 < y.count() )
                    header << ", \\";
                header << std::endl;
            }

            while ( t_mult * t_max < 1 ) {
                ++ t_scale;
                t_mult = pow( 1000, t_scale );
            }

            std::cout << "unset logscale" << std::endl;

            if ( x.log )
                std::cout << "set logscale x" << std::endl;

            double x_range = x.scaled( x.max ) - x.scaled( x.min );
            int k = 1;

            while ( x.log && log(x_range) / log(pow(x.step, k)) > 20 )
                ++ k;

            while ( !x.log && x_range / x.step * k > 10 )
                ++ k;

            /* measurement-specific */
            std::cout << "set xrange [" << x.scaled( x.min ) << ":" << x.scaled( x.max ) << "]" << std::endl
                      << "set xtics " << (x.log ? pow(x.step, k) : x.step * k) << std::endl
                      << "unset mxtics" << std::endl
                      << "set xlabel \"" << x.name << (x.unit.empty() ? "" : " [" + x.unit + "]") << "'" << std::endl
                      << "set ylabel \"time [" << time_units[ t_scale ] << "]\"" << std::endl
                      << "set title \"{/LiberationMono " << tc->group() << "::" << tc->name << "}\"" << std::endl
                      << "set key outside title '"  << y.name << (y.unit.empty() ? "" :
                                                                  " [" + y.unit + "]") << "' Left" << std::endl
                      << "set format x '%.0f'" << std::endl;

            std::cout << header.str();

            for ( int q_seq = 0; q_seq < y.count(); ++ q_seq ) {
                ResultSet res = results[ q_seq ];
                res.interpolate();

                std::stringstream raw, interp;

                for ( int p_seq = 0; p_seq < x.count(); ++ p_seq ) {
                    double p = tc->parameter( x, p_seq );
                    raw << " " << x.scaled( p ) << " " << res.sample( p, t_mult ) << std::endl;
                    interp << " " << x.scaled( p ) << " " << res.sample( p, t_mult ) << std::endl;

                    if ( p_seq < x.count() - 1 ) {
                        double p_next = tc->parameter( x, p_seq + 1 );
                        for ( int i = 1; i < 20; ++i ) {
                            double p_ = p + i * (p_next - p) / 20.0;
                            interp << " " << x.scaled( p_ )
                                   << " " << res.sample( p_, t_mult ) << std::endl;
                        }
                    }
                }

                /* each overlay needs the data to be repeated :-( */
                std::cout << interp.str() << "end" << std::endl
                          << interp.str() << "end" << std::endl
                          << interp.str() << "end" << std::endl
                          << interp.str() << "end" << std::endl
                          << raw.str()    << "end" << std::endl;
            }
        }
        done.insert( group->group() );
    }
}

template< typename T >
std::string _typeid() {
    int stat;
    return abi::__cxa_demangle( typeid( T ).name(),
                                nullptr, nullptr, &stat );
}

}

template< typename BenchGroup, void (BenchGroup::*testcase)() >
struct Benchmark : BenchmarkBase
{
    std::pair< Axis, Axis > axes() {
        BenchGroup bg;
        return std::make_pair( bg.x, bg.y );
    }

    int parameter( Axis a, int p ) {
        BenchGroup bg;
        return bg.parameter( a, p );
    }

    double normal() {
        BenchGroup bg;
        return bg.normal();
    }

    void run() {
        BenchGroup bg;
        bg.setup( p, q );
#ifdef __unix // TODO: figure out a win32 implementation
        clock_gettime( CLOCK_MONOTONIC, &start );
        (bg.*testcase)();
        clock_gettime( CLOCK_MONOTONIC , &end );
        int64_t ns = end.tv_nsec - start.tv_nsec;
        time_t s = end.tv_sec - start.tv_sec;
        if ( ns < 0 ) {
            s -= 1;
            ns += 1000000000;
        }
        std::cout << s << "." << std::setfill( '0' ) << std::setw( 9 ) << ns;
#endif
    }

    std::string group() {
        BenchGroup bg;
        if ( bg.describe().empty() )
            return _typeid< BenchGroup >();
        return bg.describe();
    }

    Benchmark() {
        p = q = 0;
        if ( !benchmarks )
            benchmarks = new std::vector< BenchmarkBase * >;
        benchmarks->push_back( this );
    }
};

#ifndef BRICK_BENCHMARK_REG

#define BENCHMARK(n) void n()

#else

#define BENCHMARK(n)                                                    \
    void __reg_ ## n() {                                                \
        using __T = typename std::remove_reference< decltype(*this) >::type; \
        ::brick::unittest::_register_g<                                 \
            ::brick::benchmark::Benchmark, __T, &__T::n, &__T::__reg_ ## n >( #n, false ); \
    }                                                                   \
    void n()

#endif

}
}

namespace brick_test {

using namespace ::brick::benchmark;

namespace benchmark {

struct SelfTest : BenchmarkGroup {
    SelfTest() {
        x.active = true;
        x.name = "items";
        x.unit = "kfrob";
        x.normalize = Axis::Div;

        y.active = true;
        y.min =      800000;
        y.max =     6400000;
        y.unit_div =   1000;
        y.log = true;
        y.step = 2;
        y.unit = "k";
    }

    BENCHMARK(empty) {}
    BENCHMARK(delay) {
        for ( int i = 0; i < p; ++i )
            for ( int j = 0; j < q; ++j );
    }
};

}
}

#endif

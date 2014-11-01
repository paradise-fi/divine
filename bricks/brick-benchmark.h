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
#include <brick-gnuplot.h>

#include <numeric>
#include <cmath>
#include <algorithm>
#include <random>

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

struct Estimate {
    double low, high, mean;
};

struct Box {
    double low, high, median, width;
};

namespace {

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
    std::mt19937 rand;
    std::uniform_int_distribution<> dist( 0, s.size() );

    Sample result;
    for ( int i = 0; i < iterations; ++i ) {
        Sample resample;
        while ( resample.size() < s.size() )
            resample.push_back( s[ dist( rand ) ] );
        result.push_back( estimator( resample ) );
    }
    return result;
}

}

struct Axis {
    bool log; /* if true, step is multiplicative, in percent */
    enum { Quantitative, Qualitative, Disabled } type;
    enum { Mult, Div, None } normalize; /* scale time per unit on this axis? */
    int64_t min, max;
    double step; // useful for log-scaled benchmarks
    double unit_mul, unit_div;
    std::string name, unit;

    std::function<std::string(int64_t)> _render;

    Axis() : log( false ), type( Disabled ), normalize( None ),
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
            default: ASSERT_UNREACHABLE( "bogus value of normalize" );
        }
    }

    int amplitude() {
        return floor( 1 + log10( scaled( max ) ) );
    }

    int count() {
        if ( type == Disabled )
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
    int fds[2];

    virtual double normal() = 0;
    virtual int parameter( Axis, int ) = 0;
    virtual std::pair< Axis, Axis > axes() = 0;
    virtual std::string describe() = 0;
    virtual std::string group() { return ""; } // TestCaseBase
    int64_t p, q;
};


struct BenchmarkGroup {
    Axis x, y; // z is time
    int64_t p, q; // parameter values on x and y axes
    struct timespec start, end;

    void reset() {
#ifdef BRICK_BENCHMARK_REG
        clock_gettime( CLOCK_MONOTONIC, &start );
#endif
    }

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
        return "x:" + x.name + " y:" + y.name +
               " x:unit:" + x.unit + " y:unit:" + y.unit;
    }
    virtual double normal() { return 1.0; }
};

namespace {

std::vector< BenchmarkBase * > *benchmarks = nullptr;

std::vector< std::string > time_units = { "s", "ms", "μs", "ns", "ps", "fs", "as", "zs", "ys" };

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
        << std::setw( 6 ) << point * mult << " "
        << std::setw( 2 ) << time_units[ scale ] << " ±"
        << std::setw( 4 ) << high_err * mult << ")";
    return str.str();
}

struct SampleStats {
    enum SampleQuality { Satisfactory, Unsatisfactory };

    // returns true if samples are satisfactory
    SampleQuality processSamples( int cutLimit = 50, int sumLimit = 5 ) {
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
            return Satisfactory; /* the confidence interval is less than 5% => good enough */

        /* if the sample is reasonably big but unsatisfactory, cut off outliers */
        if ( int( sample.size() ) > cutLimit || ( sumLimit > 0 && sum( sample ) > sumLimit ) ) {
            auto end = std::remove_if( sample.begin(), sample.end(),
                                       [&]( double n ) { return fabs(n - m_sample) > 3 * iqr; } );
            sample.erase( end, sample.end() ); // TODO: store the outliers
        }
        return Unsatisfactory;
    }

    Sample sample;

    Sample bs_median, bs_mean, bs_stddev;
    Box b_sample, b_median, b_mean, b_stddev;
    double m_sample, m_mean, m_median;
    double sd_sample;
};

void repeat( BenchmarkBase *tc, gnuplot::DataSet &res ) {
#ifdef __unix
    char buf[1024];
    ::socketpair( AF_UNIX, SOCK_STREAM, PF_UNIX, tc->fds );
#endif
    SampleStats stats;

    Axis x = tc->axes().first, y = tc->axes().second;

    int iterations = 0;

    while ( ( sum( stats.sample ) < 3 && iterations < 300 ) ||
            ( stats.sample.size() < 100 && iterations < 200 ) ) {
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
            stats.sample.push_back( time );
#endif
        }

        if ( stats.processSamples() == SampleStats::Satisfactory )
            break;
    }

    double factor = x.normal( tc->p ) * y.normal( tc->q ) * tc->normal();

    std::cerr << "  " << x.name << ": " << std::setw( x.amplitude() ) << x.render( tc->p ) << " " << x.unit
              << " "  << y.name << ": " << std::setw( y.amplitude() ) << y.render( tc->q ) << " " << y.unit
              << " μ: " << render_ci( stats.m_mean, stats.m_mean - stats.b_mean.low, stats.b_mean.high - stats.m_mean, factor )
              << " m: " << render_ci( stats.b_sample.median,
                                      stats.b_sample.median - stats.b_median.low,
                                      stats.b_median.high - stats.b_sample.median, factor )
              << " σ: " << render_ci( stats.sd_sample,
                                      stats.sd_sample - stats.b_stddev.low,
                                      stats.b_stddev.high - stats.sd_sample, factor )
              << " | n = " << std::setw( 3 ) << stats.sample.size()
              << ", bad = " << std::setw( 3 ) << iterations - stats.sample.size() << std::endl;

    res.append( x.scaled( tc->p ),
                y.scaled( stats.m_mean * factor ),
                y.scaled( stats.b_mean.low * factor ),
                y.scaled( stats.b_mean.high * factor ) );

#ifdef __unix
    ::close( tc->fds[0] );
    ::close( tc->fds[1] );
#endif
}

/* TODO duplicated from brick-shelltest */
template< typename C >
void split( std::string s, C &c, char delim = ',' ) {
    std::stringstream ss( s );
    std::string item;
    while ( std::getline( ss, item, delim ) )
        c.push_back( item );
}

struct BeginsWith {
    std::string p;
    BeginsWith( std::string p ) : p( p ) {}
    bool operator()( std::string s ) {
        return std::string( s, 0, p.size() ) == p;
    }
};

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

    Filter( int argc, const char **argv ) {
        for ( int i = 1; i < argc; ++i ) {
            if ( BeginsWith( "--" )( argv[i] ) )
                continue;
            formula.emplace_back();
            split( argv[i], formula.back() );
        }
    }
};

std::string shortdesc( std::string d, bool invert = false ) {
    std::vector< std::string > bits, keep;
    std::string res;

    split( d, bits, ' ' );
    int types = std::count_if( bits.begin(), bits.end(), BeginsWith( "type:" ) );
    std::copy_if( bits.begin(), bits.end(), std::back_inserter( keep ),
                  [ types, invert ]( std::string s ) {
                      bool v = !BeginsWith( "x:" )( s ) &&
                               !BeginsWith( "y:" )( s ) &&
                               (types == 1 || !BeginsWith( "type:" )( s ) );
                      return invert ? !v : v;
                  } );
    for ( auto k : keep )
        res += k + " ";
    return std::string( res, 0, res.length() - 1 );
}

void list( int argc, const char **argv ) {
    ASSERT( benchmarks );
    Filter flt( argc, argv );
    for ( auto tc : *benchmarks ) {
        std::string d = tc->describe();
        if ( !flt.matches( d ) )
            continue;
        std::vector< std::string > extra;
        std::cerr << "• " << shortdesc( d ) << std::endl;
        if ( !shortdesc( d, true ).empty() )
            std::cerr << "  " << shortdesc( d, true ) << std::endl;
    }
}

void run( int argc, const char **argv ) {
    ASSERT( benchmarks );

    if ( argc >= 2 && std::string( argv[1] ) == "--list" )
        return list( argc, argv );

    Filter flt( argc, argv );

    gnuplot::Plots plots;

    for ( auto tc : *benchmarks ) {
        if ( !flt.matches( tc->describe() ) )
            continue;

        gnuplot::Plot &plot = plots.append();

        std::cerr << "## " << shortdesc( tc->describe() ) << std::endl;
        auto axes = tc->axes();
        Axis x = axes.first, y = axes.second;

        for ( int q_seq = 0; q_seq < y.count(); ++ q_seq ) {
            int64_t q_val = tc->parameter( y, q_seq );
            auto &ds = plot.append( y.render( q_val ), y.type == Axis::Qualitative ? 0 : q_val,
                                    4, gnuplot::DataSet::RibbonLP );
            for ( int p_seq = 0; p_seq < x.count(); ++ p_seq ) {
                tc->p = tc->parameter( x, p_seq );
                tc->q = tc->parameter( y, q_seq );
                repeat( tc, ds );
            }
        }

        double t_max = 0, t_mult = 1;
        int t_scale = 0;

        for ( int q_seq = 0; q_seq < y.count(); ++ q_seq )
            for ( int p_seq = 0; p_seq < x.count(); ++ p_seq )
                t_max = std::max( t_max, plot[ q_seq ][ p_seq ][ 3 ] );

        while ( t_mult * t_max < 1 ) {
            ++ t_scale;
            t_mult = pow( 1000, t_scale );
        }

        if ( x.log )
            plot.logscale( gnuplot::Plot::X );

        double x_range = x.scaled( x.max ) - x.scaled( x.min );
        int k = 1;

        while ( x.log && log(x_range) / log(pow(x.step, k)) > 20 )
            ++ k;

        while ( !x.log && x_range / x.step * k > 10 )
            ++ k;

        plot.rescale  ( gnuplot::Plot::Y, t_mult );
        plot.bounds   ( gnuplot::Plot::X, x.scaled( x.min ), x.scaled( x.max ) );
        plot.interval ( gnuplot::Plot::X, x.log ? pow(x.step, k) : x.step * k );
        plot.axis     ( gnuplot::Plot::X, x.name, x.unit );
        plot.axis     ( gnuplot::Plot::Y, "time", time_units[ t_scale ] );
        plot.axis     ( gnuplot::Plot::Z, y.name, y.unit );
        plot.name     ( shortdesc( tc->describe() ) );
        switch ( y.type ) {
            case Axis::Qualitative: plot.style( gnuplot::Style::Spot ); break;
            case Axis::Quantitative: plot.style( gnuplot::Style::Gradient ); break;
            default: ;
        }
    }
    std::cout << plots.plot();
}

using brick::unittest::_typeid;

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
        clock_gettime( CLOCK_MONOTONIC, &bg.start );
        (bg.*testcase)();
        clock_gettime( CLOCK_MONOTONIC , &bg.end );
        int64_t ns = bg.end.tv_nsec - bg.start.tv_nsec;
        time_t s = bg.end.tv_sec - bg.start.tv_sec;
        if ( ns < 0 ) {
            s -= 1;
            ns += 1000000000;
        }
        std::cout << s << "." << std::setfill( '0' ) << std::setw( 9 ) << ns;
#endif
    }

    std::string describe() {
        BenchGroup bg;
        std::string d = bg.describe();
        if ( d.empty() )
            d = _typeid< BenchGroup >();
        return d + " " + "test:" + name;
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
        x.type = Axis::Quantitative;
        x.name = "items";
        x.unit = "kfrob";
        x.normalize = Axis::Div;

        y.type = Axis::Quantitative;
        y.min =      800000;
        y.max =     6400000;
        y.unit_div =   1000;
        y.log = true;
        y.step = 2;
        y.unit = "k";
    }

    std::string describe() { return "category:selftest"; }

    BENCHMARK(empty) {}
    BENCHMARK(delay) {
        for ( int i = 0; i < p; ++i )
            for ( int j = 0; j < q; ++j );
    }
};

}
}

#endif

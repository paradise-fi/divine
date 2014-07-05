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

struct BenchmarkBase : unittest::TestCaseBase {
    struct timespec start, end;
    int fds[2];
    virtual std::pair< int, int > dimensions() = 0;
    virtual void setup( int p_seq, int q_seq ) = 0;
};

struct Axis {
    bool active; /* only evaluate the axis if it is active */
    bool log; /* if true, step is multiplicative, in percent */
    bool normalize; /* scale time per unit on this axis? */
    int64_t min, max, step;
    int64_t unit_mul, unit_div;
    std::string name, unit;
    Axis() : active( false ), log( false ), normalize( false ),
             min( 1 ), max( 10 ), step( 1 ),
             unit_mul( 1 ), unit_div( 1 ),
             name( "n" ), unit( "unit" ) {}
    int count() {
        if ( !active )
            return 1;
        ASSERT_LEQ( min, max );
        if ( log ) { // use floating point ::log?
            int r = 1, n = min;
            while ( n < max ) {
                ++ r;
                n = (n * step) / 100;
            }
            return r;
        }
        return 1 + (max - min) / step;
    }
};

struct BenchmarkGroup {
    Axis x, y; // z is time
    int64_t p, q; // parameter values on x and y axes
    BenchmarkGroup() : p( 0 ), q( 0 ) {}
    virtual int64_t parameter( Axis a, int seq ) {
        if ( !a.log )
            return a.min + seq * a.step;
        for ( int i = 0; i < seq; ++i )
            a.min = (a.min * a.step) / 100;
        return a.min;
    }
};

namespace {

std::vector< BenchmarkBase * > *benchmarks = nullptr;

std::string render_ci( double point, double low_err, double high_err )
{
    int scale = 0;
    double mult = 1;

    std::stringstream str;
    std::vector< std::string > names = { "s", "ms", "μs", "ns" };

    while ( point * mult < 1 &&
            low_err * mult < 0.01 &&
            high_err * mult < 0.01 ) {
        ++ scale;
        mult = pow( 1000, scale );
    }

    str << std::fixed << std::setprecision( 2 ) << std::setw( 6 )
        << point * mult << " " << names[ scale ] << " ± ("
        << std::setw( 4 ) << low_err * mult << " "
        << std::setw( 4 ) << high_err * mult << ")";
    return str.str();
}

void repeat( BenchmarkBase *tc ) {
#ifdef __unix
    char buf[1024];
    ::socketpair( AF_UNIX, SOCK_STREAM, PF_UNIX, tc->fds );
#endif
    Sample sample;
    int64_t p = 0, q = 0;
    std::string p_unit, q_unit, p_name, q_name;

    Sample bs_median, bs_mean, bs_stddev;
    Box b_sample, b_median, b_mean, b_stddev;
    double m_sample, m_mean, m_median;
    double sd_sample;

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
            parse >> p_name >> p >> p_unit
                  >> q_name >> q >> q_unit
                  >> time;
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

    std::cerr << "    " << p_name << " = " << std::setw( 8 ) << p << " " << p_unit
              <<   ", " << q_name << " = " << std::setw( 8 ) << q << " " << q_unit
              << ", mean = "   << render_ci( m_mean, m_mean - b_mean.low, b_mean.high - m_mean )
              << ", median = " << render_ci( b_sample.median,
                                             b_sample.median - b_median.low,
                                             b_median.high - b_sample.median )
              << ", σ = "      << render_ci( sd_sample,
                                             sd_sample - b_stddev.low,
                                             b_stddev.high - sd_sample )
              << " | n = " << std::setw( 3 ) << sample.size()
              << ", discarded = " << std::setw( 3 ) << iterations - sample.size() << std::endl;

    ::close( tc->fds[0] );
    ::close( tc->fds[1] );
}

void run( int argc, const char **argv ) {
    ASSERT( benchmarks );
    std::set< std::string > done;

    for ( auto group : *benchmarks ) {
        if ( done.count( group->group() ) )
            continue;
        for ( auto tc : *benchmarks ) {
            if ( group->group() != tc->group() )
                continue;
            std::cerr << tc->group() << " " << tc->name << ":" << std::endl;
            auto dim = tc->dimensions();
            for ( int p_seq = 0; p_seq < dim.first; ++ p_seq )
                for ( int q_seq = 0; q_seq < dim.second; ++ q_seq ) {
                    tc->setup( p_seq, q_seq );
                    repeat( tc );
                }
        }
        done.insert( group->group() );
    }
}

}

template< typename BenchGroup, void (BenchGroup::*testcase)() >
struct Benchmark : BenchmarkBase
{
    int64_t p, q;

    std::pair< int, int > dimensions() {
        BenchGroup bg;
        return std::make_pair( bg.x.count(), bg.y.count() );
    }

    void setup( int p_seq, int q_seq ) {
        BenchGroup bg;
        p = bg.parameter( bg.x, p_seq );
        q = bg.parameter( bg.y, q_seq );
    }

    void run() {
        BenchGroup bg;
        bg.p = p;
        bg.q = q;
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
        std::cout << bg.x.name << " " << (bg.p * bg.x.unit_mul) / bg.x.unit_div << " " << bg.x.unit << " "
                  << bg.y.name << " " << (bg.q * bg.y.unit_mul) / bg.y.unit_div << " " << bg.y.unit << " "
                  << s << "." << std::setfill( '0' ) << std::setw( 9 ) << ns;
#endif
    }

    std::string group() {
        int stat;
        return abi::__cxa_demangle( typeid( BenchGroup ).name(), nullptr, nullptr, &stat );
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
        x.normalize = true;

        y.active = true;
        y.min =      800000;
        y.max =     6400000;
        y.unit_div =   1000;
        y.log = true;
        y.step = 200; // percent
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

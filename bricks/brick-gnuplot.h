// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * Make pretty plots with gnuplot. Three axes are supported, x and y being the
 * planar axes of the plot and z being different linestyles (colors). Rendering
 * of confidence intervals as ribbons is supported. The result is a single
 * self-contained file that can be piped into gnuplot for rendering. The y
 * values in-between samples are interpolated using cubic splines (when
 * plotting with lines, that is -- points-only plotting is supported as well).
 *
 * The API is not intended to expose full power of gnuplot. It just provides a
 * convenient way to build and render a specific type of x-y plots.
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

#ifndef BRICK_GNUPLOT_H
#define BRICK_GNUPLOT_H

namespace brick {
namespace gnuplot {

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

    template< typename... Ts >
    void pushRow( Ts... v ) {
        std::vector< double > row{ v... };
        ASSERT_EQ( row.size(), width );
        std::copy( row.begin(), row.end(), std::back_inserter( m ) );
    };
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

using Style = std::vector< std::string >; // just a list of colors

namespace {

const Style rainbow = { "rgb '#ff4500'", "rgb '#ffa500'", "rgb '#006400'",
                        "rgb '#0000ff'", "rgb '#9400d3'", "rgb '#800000'",
                        "rgb '#ff0000'" };

}

/* a single line of a plot */
struct DataSet {
    Matrix _raw;
    std::vector< Spline > _interpolated;

    enum Style { Points, LinePoints, Line, Ribbon,
                 RibbonLine, RibbonLP } _style;
    std::string _name;

    bool points() {
        return _style != Ribbon && _style != Line;
    }

    bool lines() {
        return _style == LinePoints || _style == RibbonLine ||
               _style == RibbonLP;
    }

    bool ribbon() {
        return _style == Ribbon || _style == RibbonLine ||
               _style == RibbonLP;
    }

    template< typename... Ts >
    void append( Ts... ts ) {
        _raw.pushRow( ts... );
    }

    Matrix::Proxy operator[]( int i ) {
        return _raw[ i ];
    }

    std::string data( double xscale, double yscale )
    {
        if ( _interpolated.empty() ) {
            _interpolated.resize( _raw.width );
            for ( int c = 1; c < _raw.width; ++c )
                for ( int r = 0; r < _raw.height(); ++r )
                    _interpolated[c].push( _raw[r][0], _raw[r][c] );
            for ( int c = 1; c < _raw.width; ++c )
                _interpolated[c].interpolateNaturalKs();
        }

        std::stringstream str;
        for ( int r = 0; r < _raw.height() - 1; ++r )
            for ( int k = 0; k < 20; ++k ) {
                double x = _raw[r][0], x_next = _raw[r + 1][0];
                x = x + k * (x_next - x) / 20.0;
                str << x * xscale;
                for ( int c = 1; c < _raw.width; ++c )
                    str << " " << _interpolated[c].eval( x ) * yscale;
                str << std::endl;
            }
        str << " " << _raw[_raw.height() - 1][0] * xscale;
        for ( int c = 1; c < _raw.width; ++c )
            str << " " << _raw[_raw.height() - 1][c] * yscale;
        str << std::endl << "end" << std::endl;
        return str.str();
    }

    std::string rawdata( double xscale, double yscale ) {
        std::stringstream str;
        for ( int r = 0; r < _raw.height(); ++r ) {
            str << " " << _raw[r][0] * xscale;
            for ( int c = 1; c < _raw.width; ++c )
                str << " " << _raw[r][c] * yscale;
            str << std::endl;
        }
        str << "end" << std::endl;
        return str.str();
    }

    DataSet( int w ) : _raw( 0, w ) {}
};

struct Plot {
    enum Terminal { PDF } _terminal;
    enum Axis { X, Y, Z };
    std::string _font, _name;
    Style _style;

    using Bounds = std::pair< double, double >;

    std::map< Axis, std::string > _units;
    std::map< Axis, std::string > _names;
    std::map< Axis, Bounds > _bounds;
    std::map< Axis, double > _interval;
    std::map< Axis, double > _rescale;
    std::set< Axis > _logscale;

    std::vector< DataSet > _datasets;

    void bounds( Axis a, double min, double max ) {
        _bounds[ a ] = std::make_pair( min, max );
    }

    void axis( Axis a, std::string n, std::string u ) {
        _names[ a ] = n;
        if ( !u.empty() )
            _units[ a ] = u;
    }

    void rescale( Axis a, double f ) { _rescale[ a ] = f; }
    void logscale( Axis a ) { _logscale.insert( a ); }
    void interval( Axis a, double i ) { _interval[ a ] = i; }
    void name( std::string n ) { _name = n; }

    DataSet &append( std::string name, int cols, DataSet::Style s ) {
        _datasets.emplace_back( cols );
        auto &n = _datasets.back();
        n._style = s;
        n._name = name;
        return n;
    }

    DataSet &operator[]( int i ) { return _datasets[ i ]; }

    Plot() : _terminal( PDF ), _font( "Liberation Sans,10" ),
             _style( rainbow )
    {}

    std::string preamble()
    {
        std::stringstream str;

        str << "set terminal pdfcairo font '" << _font << "'" << std::endl;

        int i = 1;
        for ( auto c : _style ) {
            str << "set style line " << i
                << " lc " << c << " lt 1 lw 2" << std::endl;
            ++ i;
        }

        str << "set style fill transparent solid 0.3" << std::endl;

        str << "set style line 20 lc rgb '#808080' lt 1" << std::endl
            << "set border 3 back ls 20" << std::endl
            << "set tics nomirror out scale 0.75" << std::endl
            << "set style line 21 lc rgb'#808080' lt 0 lw 1" << std::endl
            << "set grid back ls 21" << std::endl
            << "set arrow from graph 1,0 to graph 1.05,0 "
            << "size screen 0.025,10,60 filled ls 20" << std::endl
            << "set arrow from graph 0,1 to graph 0,1.05 "
            << "size screen 0.025,10,60 filled ls 20" << std::endl;

        return str.str();
    }

    std::string setupAxis( Axis a ) {
        std::stringstream str;
        char l = a == X ? 'x' : 'y';

        if ( _bounds.count( a ) )
            str << "set " << l << "range [" << _bounds[ a ].first
                << ":" << _bounds[ a ].second << "]" << std::endl;

        if ( _interval.count( a ) )
            str << "set " << l << "tics " << _interval[ a ] << std::endl;
        str << "unset m" << l << "tics" << std::endl;

        if ( _names.count( a ) )
            str << "set " << l << "label '" << _names[ a ]
                << (_units.count( a ) ? " [" + _units[ a ] + "]" : "")
                << "'" << std::endl;

        str << (_logscale.count( a ) ? "set" : "unset") << " logscale " << l << std::endl;
        return str.str();
    }

    std::string setup() {
        std::stringstream str;
        str << setupAxis( X ) << setupAxis( Y )
            << "set title '" << _name << "'" << std::endl
            << "set key outside title '"  << _name[ Z ]
            << (_units.count( Z ) ? " [" + _units[ Z ] + "]" : "")
            << "' Left" << std::endl
            << "set format x '%.0f'" << std::endl;
        return str.str();
    }

    std::string datasets() {
        std::stringstream str;
        int seq = 1;
        str << "plot \\" << std::endl;
        for ( auto d : _datasets ) {
            if ( d.ribbon() )
                str << " '-' using 1:3:4 title '" << d._name << "' with filledcurves ls " << seq
                    << ",\\\n '-' using 1:3 notitle with lines ls " << seq << " lw 0.5"
                    << ",\\\n '-' using 1:4 notitle with lines ls " << seq << " lw 0.5";
            if ( d.lines() )
                str << ",\\\n '-' using 1:2 notitle with lines ls " << seq;
            if ( d.points() )
                str << ",\\\n '-' using 1:2 notitle with points ls " << seq
                    << " pt 7 ps 0.1 lc rgb '#000000'";
            if ( seq != _datasets.size() )
                str << ", \\" << std::endl << "  \\" << std::endl;
            ++ seq;
        }

        str << std::endl;

        for ( auto d : _datasets ) {
            double xsc = _rescale.count( X ) ? _rescale[ X ] : 1;
            double ysc = _rescale.count( Y ) ? _rescale[ Y ] : 1;
            auto data = d.data( xsc, ysc );
            auto rdata = d.rawdata( xsc, ysc );

            if ( d.ribbon() )
                str << data << data << data;
            if ( d.lines() )
                str << data;
            if ( d.points() )
                str << rdata;
        }
        return str.str();
    }

    std::string plot() {
        return preamble() + setup() + datasets();
    }
};

struct Plots {
    std::vector< Plot > _plots;
    Plot &append() {
        _plots.emplace_back();
        return _plots.back();
    }
};

}
}

#endif

// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab

// -*- C++ -*-
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <set>
#include <unordered_map>
#include <queue>
#include <limits>
#include <sstream>
#include <dbm/fed.h>
#include <dbm/constraints.h>
#include <dbm/print.h>
#include <brick-assert.h>

#ifndef DIVINE_TIMED_URA_H
#define DIVINE_TIMED_URA_H

namespace divine {
namespace timed {

namespace Ura {

static const int inf = std::numeric_limits< int >::max();

struct guard {
    // simple guard of the form  "< x" or "<= x"
    int val;
    bool open; // true: <, false: <=
    guard( int v, bool o ) : val( v ), open( o ) {}
    guard() : val( 0 ), open( false ) {}

    bool operator <( const guard &other ) const {
        if ( val < other.val ) return true;
        if ( val > other.val ) return false;
        return open > other.open; // open && !other.open
    }

    guard operator +( guard rhs ) const {
        if ( rhs.val == inf || val == inf )
            rhs.val = inf;
        else {
            rhs.val += val;
            rhs.open = rhs.open || open;
        }
        return rhs;
    }

    bool operator ==( const guard &other ) const {
        return val == other.val && open == other.open;
    }

    friend std::ostream &operator <<( std::ostream &out, const guard &g ) {
        out << ( g.open ? " <" : "<=" );
        if ( g.val == inf ) out << "inf"; else out << g.val;
        return out;
    }

    static guard less_than( int val ) {
        return { val, true };
    }

    static guard less_eq( int val ) {
        return { val, false };
    }

};

struct interval_partition {
    // partition of the (non-negative) real line into intervals
    // (n+1) intervals represented by n guards
    std::vector< guard > guards;
    bool from_zero = true;

    interval_partition() {}

    void output_interval( int index, std::ostream &out ) {
        if ( index == 0 ) {
            out << ( from_zero ? "[0," : "(-inf," );
        } else {
            auto & prev = guards[ index - 1 ];
            out << ( prev.open ? '[' : '(' ) << prev.val << ',';
        }
        if ( index == last() ) {
            out << "+inf)";
        } else {
            auto & next = guards[ index ];
            out << next.val << ( next.open ? ')' : ']' );
        }
    }

    void add_guard( const guard g ) {
        if ( std::find( guards.begin(), guards.end(), g ) == guards.end() )
            guards.push_back( g );
    }

    std::pair< guard, guard > to_dbm_guards( int index ) {
        std::pair< guard, guard > ret;
        // change (x - y) \in interval into
        // (y - x) <? ret.first
        // (x - y) <? ret.second
        if ( index == 0 ) {
            ret.first = ( from_zero ? guard::less_eq( 0 ) : guard::less_than( inf ) );
        } else {
            ret.first = guards[ index - 1 ];
            ret.first.val = -ret.first.val;
            ret.first.open = !ret.first.open;
        }
        if ( index == last() ) {
            ret.second = guard::less_than( inf );
        } else {
            ret.second = guards[ index ];
        }
        return ret;
    }

    // index of the last interval
    int last() {
        return int( guards.size() );
    }

    // returns 0 if value is in the interval (given by index)
    // returns -1 if value is less than all values in the given interval
    // returns 1 if value is greater than all values in the given interval
    // (needed by sooner_preorder())
    int compare( int value, int index ) {
        if ( index == 0 && from_zero && value < 0 )
            return -1;
        if ( index > 0 && guard::less_than( value ) < guards[ index - 1 ] )
            return -1;
        if ( index < int( guards.size() )
                && !( guard::less_than( value ) < guards[ index ] ) )
            return 1;
        return 0;
    }

    friend std::ostream &operator <<( std::ostream &out,
                                      const interval_partition &p ) {
        out << ( p.from_zero ? "[0," : "(-inf," );
        for ( auto &g : p.guards ) {
            out << g.val << ( g.open ? ")[" : "](" ) << g.val << ',';
        }
        out << "+inf)";
        return out;
    }

    static interval_partition make_diagonal_partition( const interval_partition &px,
                                                const interval_partition &py ) {
        interval_partition ret;
        std::set< int > aux;
        for ( auto &x : px.guards ) {
            for ( auto &y : py.guards ) {
                aux.insert( x.val - y.val );
            }
        }

        for ( auto val : aux ) {
            ret.guards.push_back( guard::less_than( val ) );
            ret.guards.push_back( guard::less_eq( val ) );
        }

        ret.from_zero = false;
        return ret;
    }
};

struct dbm {
    int size;
    guard *matrix;
    dbm( int s ) : size( s ), matrix( new guard[ size * size ]() ) {}
    dbm( const dbm &other )
        : size( other.size ), matrix( new guard[ size * size ]() ) {
        std::copy( other.matrix, other.matrix + size*size, matrix );
    }
    dbm( dbm && other ) : size( other.size ), matrix ( other.matrix ) {
            other.matrix = nullptr;
    }
    guard &operator()( int x, int y ) {
        return matrix[ x + y * size ];
    }
    const guard &operator()( int x, int y ) const {
        return matrix[ x + y * size ];
    }
    ~dbm() { delete [] matrix; }
    void dump( std::ostream &out ) const {
        for ( int i = 0; i < size; ++i ) {
            for ( int j = 0; j < size; ++j ) {
                std::stringstream s;
                s << (*this)( i, j );
                out << std::setw( 7 ) << std::left << s.str();
              }
            out << std::endl;
        }
    }

    void minimize() {
        // Floyd-Warshall, yay
        for ( int k = 0; k < size; ++k )
            for ( int i = 0; i < size; ++i )
                for ( int j = 0; j < size; ++j ) {
                    guard aux = (*this)( i, k ) + (*this)( k, j );
                    if ( aux < (*this)( i, j ) )
                        (*this)( i, j ) = aux;
                }
    }

    bool neg_diagonal() const {
        for ( int i = 0; i < size; ++i ) {
            if ( (*this)( i, i ) < guard::less_eq( 0 ) ) return true;
        }
        return false;
    }

    bool empty() {
        minimize();
        return neg_diagonal();
    }

    void intersect( const dbm &other ) {
        for ( int i = 0; i < size; ++i )
            for ( int j = 0; j < size; ++j )
                if ( other( i, j ) < (*this)( i, j ) )
                    (*this)( i, j ) = other( i, j );
    }
};

struct ura {
    //std::vector< std::string > clock_names;
    std::vector< interval_partition > clock_guards;
    std::vector< std::vector< interval_partition > > diagonal_guards;
    // diagonal_guards[ y ][ x ] contains the partition for x - y
    int num_clocks = 0;

    void add_guard( unsigned clock, int value, bool open ) {
        ASSERT_LEQ( clock, clock_guards.size() - 1 );
        clock_guards[ clock ].add_guard( guard( value, open ) );
    }

    void set_dimension( int dim ) {
        clock_guards.resize( dim );

        num_clocks = dim;
    }

    void finalize() {
        if ( !dbm_rep.empty() ) {
            dbm_rep.clear();
            transitions.clear();
            uppaal_dbm_rep.clear();
            diagonal_guards.clear();
            visited.clear();
        }

        for ( int clk = 0; clk < num_clocks; ++clk )
            std::sort( clock_guards[ clk ].guards.begin(), clock_guards[ clk ].guards.end() );
        for ( int i = 0; i < num_clocks; ++i ) {
            diagonal_guards.emplace_back();
            auto &diag = diagonal_guards.back();
            for ( int j = 0; j < i; ++j ) {
                diag.push_back( interval_partition::make_diagonal_partition( clock_guards[ j ],
                                                                             clock_guards[ i ] ) );
            }
        }
        build_ura();
    }

    typedef std::vector< int > ultraregion;

    int diag_index( int x, int y ) {
        // assume x < y
        // compute index of diagonal x-y in ultraregion vector
        return num_clocks + (y-x-1 + x * (num_clocks - 1) - x * (x-1) / 2);
    }

    ultraregion get_initial() {
        ultraregion ur( num_clocks );
        // ur.reserve( ... );
        for ( int i = 0; i < num_clocks; ++i ) {
            for ( int j = i + 1; j < num_clocks; ++j ) {
                auto &diag = diagonal_guards[ j ][ i ].guards;
                if ( diag.size() == 0 )
                    ur.push_back( -1 );
                else {
                    auto it = std::lower_bound( diag.begin(), diag.end(),
                                                guard::less_eq( 0 ) );
                    ur.push_back( it - diag.begin() );
                }
            }
        }
        return ur;
    }

    void output_ura( std::ostream &out ) {
        std::vector< ultraregion > aux( dbm_rep.size() );
        for ( auto &x : visited ) {
            if ( x.second != -1 )
                aux[ x.second ] = x.first;
        }

        for ( int i = 0; i < int( aux.size() ); ++i ) {
            out << "----- " << i << " -----\n";
            dump_ultraregion( aux[ i ], out );
        }

        out << "---------------\n";

        for ( auto &t : transitions ) {
            int from, to;
            unsigned c;
            std::tie( from, c, to ) = t;

            out << "\t" << from << " -> " << to << " (";
            if ( c == 0U )
                out << "delay";
            else {
                out << "reset";
                for ( int x = 0; x < num_clocks; ++x ) {
                    if ( c & (1U << x) )
                        out << ' ' << x;
                }
            }
            out << ")\n";
        }
    }

    void dump_ultraregion( const ultraregion &ur, std::ostream &out ) {
        int k = 0;
        for ( int i = 0; i < num_clocks; ++i, ++k ) {
            out << i << " in ";
            clock_guards[ i ].output_interval( ur[ k ], out );
            out << std::endl;
        }
        for ( int i = 0; i < num_clocks; ++i ) {
            for ( int j = i + 1; j < num_clocks; ++j, ++k ) {
                if ( ur[ k ] == -1 ) continue; // no constraint
                out << i << '-' << j << " in ";
                diagonal_guards[ j ][ i ].output_interval( ur[ k ], out );
                out << std::endl;
            }
        }
    }



    void dump_partitions( std::ostream &out ) {
        for ( int i = 0; i < num_clocks; ++i ) {
            out << i << ": " << clock_guards[ i ] << std::endl;
        }
        for ( int i = 0; i < num_clocks; ++i ) {
            for ( int j = i + 1; j < num_clocks; ++j ) {
                out << i << '-' << j << ": "
                    << diagonal_guards[ j ][ i ] << std::endl;
            }
        }
    }

    // --- successor region computation ---

    // preorder < on clocks "leaves its interval sooner than"
    // returns -1 if x < y, 0 if x ~ y, 1 if x > y
    // only applicable to bounded clocks
    int sooner_preorder( const ultraregion &ur, int x, int y ) {
        int ux = ur[ x ];
        int uy = ur[ y ];
        int uxy = ur[ diag_index( x, y ) ];

        // endpoints
        auto &ex = clock_guards[ x ].guards[ ux ];
        auto &ey = clock_guards[ y ].guards[ uy ];

        int diff = ex.val - ey.val;
        int cmp  = diagonal_guards[ y ][ x ].compare( diff, uxy );
        if ( cmp == 0 ) { // diff in U_xy
            return ( ex.open == ey.open ? 0 :
                     ex.open ? -1 : 1 );
            // clocks in open intervals leave them sooner
        } else {
            return cmp;
        }
    }

    // find the minimal elements in the `sooner' preorder
    std::vector< int > find_soonest_clocks( const ultraregion &ur ) {
        std::vector< int > minimal;
        for ( int i = 0; i < num_clocks; ++i ) {
            // ignore unbounded clocks
            if ( ur[ i ] == clock_guards[ i ].last() ) continue;
            if ( minimal.empty() ) {
                minimal.push_back( i );
                continue;
            }

            // compare arbitrary representant of current minimal
            // vector of clocks with the new candidate
            int cmp = sooner_preorder( ur, minimal.front(), i );
            if ( cmp == 0 ) { // they are equal
                minimal.push_back( i );
            } else if ( cmp == 1 ) {
                // throw away minimal, i is less than all of them
                minimal.clear();
                minimal.push_back( i );
            }
        }
        return minimal;
    }

    ultraregion get_succ( ultraregion ur ) {
        auto to_move = find_soonest_clocks( ur );
        for ( int x : to_move ) {
            ++ur[ x ]; // advance the clock to next interval
            if ( ur[ x ] == clock_guards[ x ].last() ) {
                // x has become unbounded
                // throw away all diagonal constraints referencing x
                for ( int i = 0; i < num_clocks; ++i ) {
                    if ( i == x ) continue;
                    int l, r;
                    std::tie( l, r ) = std::minmax( i, x );
                    ur[ diag_index( l, r ) ] = -1;
                    // -1 represents no constraints
                }
            }
        }
        return ur;
    }
    // ------------------------------------

    dbm ur_to_dbm( const ultraregion &ur ) {
        dbm ret( num_clocks + 1 );
        int k = 0;
        for ( int i = 0; i < num_clocks; ++i, ++k ) {
            std::tie( ret( 0, i+1 ), ret( i+1, 0 ) ) =
                clock_guards[ i ].to_dbm_guards( ur[ k ] );
        }
        for ( int i = 0; i < num_clocks; ++i ) {
            for ( int j = i + 1; j < num_clocks; ++j, ++k ) {
                if ( ur[ k ] == -1 ) { // no constraint
                    ret( i+1, j+1 ) = ret( j+1, i+1 ) = guard::less_than( inf );
                } else {
                    std::tie( ret( j+1, i+1 ), ret( i+1, j+1 ) ) =
                        diagonal_guards[ j ][ i ].to_dbm_guards( ur[ k ] );
                }
            }
        }
        return ret;
    }

    ::dbm::dbm_t to_uppaal_dbm( const dbm &ur_dbm ) {
        ::dbm::dbm_t result( num_clocks + 1 );
        result.setInit();

        for ( int i = 0; i < num_clocks + 1; ++i ) {
            for ( int j = 0; j < num_clocks + 1; ++j ) {
                const guard &g = ur_dbm( i, j );
                if ( g.val != inf ) {
                    constraint_t constraint( i, j, g.val, g.open );
                    result &= constraint;
                }
            }
        }

        return std::move( result );
    }

    std::vector< ultraregion > reset( ultraregion ur, dbm d,
                                      const std::vector< int > &r_clocks ) {
        // assume d is minimized
        std::vector< ultraregion > ret;
        std::vector< bool > r( num_clocks, false );
        for ( int x : r_clocks ) {
            d( x+1, 0 ) = guard::less_eq( 0 );
            d( 0, x+1 ) = guard::less_eq( 0 );
            r[ x ] = true;
            ur[ x ] = 0;
        }
        for ( int y = 0; y < num_clocks; ++y ) {
            for ( int x : r_clocks ) {
                if ( x != y ) {
                    d( x+1, y+1 ) = d( 0, y+1 );
                    d( y+1, x+1 ) = d( y+1, 0 );
                }
            }
        }
        // d now represents the set of valuations immediately after reset

        std::vector< std::pair< int, int > > diagonals;
        for ( int i = 0; i < num_clocks; ++i ) {
            for ( int j = i + 1; j < num_clocks; ++j ) {
                if ( ur[ i ] == clock_guards[ i ].last()
                    || ur [ j ] == clock_guards[ j ].last() ) {
                    diagonals.emplace_back( -1, -1 );//no diagonal constraints
                } else if ( !r[ i ] && !r[ j ] ) {
                    diagonals.emplace_back( ur[ diag_index( i, j ) ],
                                            ur [ diag_index( i, j ) ] );
                    // we do not change the diagonals between non-reset clocks
                } else {
                    guard l = d( j+1, i+1 ); // upper bound on j - i
                    guard u = d( i+1, j+1 ); // upper bound on i - j
                    l.val = -l.val; // now it's lower bound on i - j
                    l.open = ! l.open;
                    auto &diag = diagonal_guards[ j ][ i ].guards;
                    // find all diagonal intervals intersecting l -- u
                    auto lb = std::upper_bound( diag.begin(), diag.end(), l );
                    auto ub = std::lower_bound( diag.begin(), diag.end(), u );
                    int li = lb - diag.begin();
                    int ui = ub - diag.begin();
                    diagonals.emplace_back( li, ui );
                }
            }
        }

        // recursively create all ultraregions covering the dbm
        if ( num_clocks > 1 )
            _gen_covering( diagonals, ret, ur, 0, 1, d );

        return ret;
    }

    void _gen_covering( const std::vector< std::pair< int, int > > &diagonals,
                        std::vector< ultraregion > &vec,
                        ultraregion &tmp, int x, int y,
                        const dbm & test_dbm ) {
        if ( num_clocks == 1 ) {
            vec.push_back( tmp );
            return;
        }

        int ix = diag_index( x, y ) - num_clocks;
        int l = diagonals[ ix ].first;
        int r = diagonals[ ix ].second;
        ix += num_clocks;

        for ( int i = l; i <= r; ++i ) {
            tmp[ ix ] = i;
            // next clock pair
            int nx = x; 
            int ny = y + 1;
            if ( ny == num_clocks ) {
                ++nx; ny = nx + 1;
            }
            if ( nx == num_clocks - 1 ) { // no more diagonals to consider
                // test whether the ultraregion has nonempty intersection
                // with the after-reset valuations
                dbm d = ur_to_dbm( tmp );
                d.intersect( test_dbm );
                if ( !d.empty() )
                    vec.push_back( tmp );
            } else {
                _gen_covering( diagonals, vec, tmp, nx, ny, test_dbm );
            }
        }
    }

    struct ur_hash {
        // this is a pretty stupid hash, but okay for now...
        size_t operator()( const ura::ultraregion &ur ) const {
            size_t ret = std::hash< int >()( ur.size() );
            for ( int i : ur ) {
                ret ^= std::hash< int >()( i );
            }
            return ret;
        }
    };

    std::vector< dbm > dbm_rep;         // dbm representation of ultraregions
    std::vector< ::dbm::dbm_t > uppaal_dbm_rep;
    std::unordered_map< ultraregion, int, ur_hash > visited;
                                                    // map to index in dbm_rep
                                                    // -1 means empty ur
    std::vector< std::tuple< int, unsigned, int > > transitions;
    // (a, b, c) \in transitions: a -> b via succ if c == 0
    // if c > 0, a -> b via reset clocks represented by c's bit pattern

    decltype( transitions )::const_iterator get_first_transition( int from, unsigned reset_mask = 0 ) const {
        auto it = transitions.begin();
        auto iE = transitions.end();
        for ( ; it != iE; ++it ) {
            if (    std::get<0>( *it ) == from
                 && std::get<1>( *it ) == reset_mask )
                return it;
        }
        return iE;
    }


    void build_ura() {
        // BFS
        std::queue< ultraregion > Q;

        _visit( get_initial(), Q );
        while ( !Q.empty() ) {
            auto u = std::move( Q.front() );
            Q.pop();
            int from = visited[ u ];

            // (delay) succesor
            int to = _visit( get_succ( u ), Q );
            // assert to != -1      // successor should be always valid
            transitions.emplace_back( from, 0U, to );

            // reset
            for ( unsigned c = 1; c < (1U << num_clocks); ++c ) {
                std::vector< int > r_clocks; // possibly ineffective
                for ( int x = 0; x < num_clocks; ++x ) {
                    if ( c & (1U << x) ) r_clocks.push_back( x );
                }
                auto urs = reset( u, dbm_rep[ from ], r_clocks );
                // assert !urs.empty()
                for ( auto &v : urs ) {
                    to = _visit( std::move( v ), Q );
                    transitions.emplace_back( from, c, to );
                }
            }
        }
        sort( transitions.begin(), transitions.end() ); // make it tidy
    }

    // returns the index into dbm_rep
    int _visit( ultraregion u, std::queue< ultraregion > &Q ) {
        // called whenever an ultraregion is found
        auto ins = visited.emplace( u, dbm_rep.size() );
        if ( !ins.second ) // we already saw this one
            return ins.first->second;

        dbm d = ur_to_dbm( u );
        d.minimize();

        // we don't have to check if d is nonempty
        // all the operations ensure that we only get valid ultraregions

        dbm_rep.push_back( d );
        uppaal_dbm_rep.push_back( to_uppaal_dbm( std::move( d ) ) );
        Q.push( std::move( u ) );
        return dbm_rep.size() - 1;
    }

    void statistics( std::ostream &out ) {
        out << "----------------------------------------"
            << "\n no. of ultraregions: " << dbm_rep.size()
            << "\n  no. of transitions: " << transitions.size()
            << "\n----------------------------------------"
            << std::endl;
    }
};
}

}
}

#endif

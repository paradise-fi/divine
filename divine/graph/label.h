// -*- C++ -*- (c) 2014 Vladimír Štill <xstill@fi.muni.cz>

#include <vector>

#include <brick-types.h>
#include <brick-unittest.h>

#include <divine/graph/probability.h>

#ifndef DIVINE_TOOLKIT_LABEL_H
#define DIVINE_TOOLKIT_LABEL_H

namespace divine {
namespace graph {

struct NoLabel : brick::types::Comparable {
    NoLabel() {}
    NoLabel( int ) {}
    NoLabel levelup( int ) const { return NoLabel(); }
    NoLabel operator *( std::pair< int, int > ) const { return NoLabel(); }
    bool operator<=( NoLabel ) const { return true; }
};

template< typename BS >
typename BS::bitstream &operator<<( BS &bs, const NoLabel & ) {
    return bs;
}
template< typename BS >
typename BS::bitstream &operator>>( BS &bs, NoLabel & ) {
    return bs;
}

struct ControlLabel {
    ControlLabel() : tid( -1 ) { }
    ControlLabel( int tid ) : tid( tid ) { }
    ControlLabel levelup( int ) { return *this; }
    ControlLabel operator *( std::pair< int, int > ) const { return *this; }
    int tid;
};

template< typename BS >
typename BS::bitstream &operator<<( BS &bs, const ControlLabel &cl ) {
    bs << cl.tid;
    return bs;
}
template< typename BS >
typename BS::bitstream &operator>>( BS &bs, ControlLabel &cl ) {
    bs >> cl.tid;
    return bs;
}

/* unified label data structure, can be converted to Probability,
 * ControlLabel or NoLablel
 */
struct Label {

    Label() : Label( 0 ) { }
    Label( int tid ) : Label( tid, 0, 0, { } ) { }
    Label( int tid, int numerator, int denominator, std::vector< int > choices ) :
        tid( tid ), numerator( numerator ), denominator( denominator ),
        choices( choices )
    { }

    Label operator*( std::pair< int, int > x ) const {
        return Label( tid, numerator * x.first, denominator * x.second, choices );
    }

    Label levelup( int c ) const {
        Label l = *this;
        l.choices.push_back( c );
        return l;
    }

    explicit operator NoLabel() const { return NoLabel(); }

    explicit operator ControlLabel() const {
        return ControlLabel( tid );
    }

    explicit operator Probability() const {
        ASSERT_LEQ( 0, denominator );
        Probability p{ int( pow( 2, tid + 1 ) ), numerator, denominator };
        ASSERT_LEQ( p.numerator, numerator );
        ASSERT_EQ( int( p.denominator ), denominator );
        for ( int c : choices )
            p = p.levelup( c );
        return p;
    }

    int tid;
    int numerator;
    int denominator;
    std::vector< int > choices;
};


static inline std::string showLabel( NoLabel ) { return ""; }
static inline std::string showLabel( ControlLabel cl ) {
    return "tid = " + std::to_string( cl.tid );
}
static inline std::string showLabel( Probability p ) { return p.text(); }

} // namespace toolkit
} // namespace divine

#endif // DIVINE_TOOLKIT_LABEL_H

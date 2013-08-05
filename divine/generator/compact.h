// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <divine/generator/common.h>
#include <divine/compact/compact.h>
#include <functional>

#ifndef DIVINE_GENERATOR_COMPACT_H
#define DIVINE_GENERATOR_COMPACT_H

namespace divine {
namespace generator {

namespace {
    template< typename Label >
    void showLabel( std::stringstream &ss, const Label &l ) {
        ss << " [ " << l << " ]";
    }

    template<>
    void showLabel< wibble::Unit >( std::stringstream &ss, const wibble::Unit & ) { }
}

template< typename _Label >
struct _Compact : public Common< Blob > {

    using Node = Blob;
    using Label = _Label;
    using Common = generator::Common< Blob >;

    struct EdgeSpec : public std::tuple< int64_t, Label > {
        int64_t node() { return std::get< 0 >( *this ); };
        Label label() { return std::get< 1 >( *this ); };

        EdgeSpec() = default;
        EdgeSpec( Node n, Label l ) : std::tuple< Node, Label >( n, l ) { }
    };

    int64_t index( Node n ) {
        return *reinterpret_cast< int64_t * >(
                this->pool().dereference( n ) + _slack );
    }

    void read( std::string file, std::vector< std::string > definitions,
            _Compact *c = nullptr )
    {
        if ( c )
            compact = c->compact;
        else
            compact.open( file );
    }

    template< typename Yield >
    void successors( Node from, Yield yield ) {
        assert( this->pool().valid( from ) );
        _successors( index( from ), yield );
    }

    template< typename Yield >
    void initials( Yield yield ) {
        _successors( 0, std::bind( yield, Node(),
                    std::placeholders::_1, std::placeholders::_2 ) );
    }

    bool isAccepting( Node n ) { return false; } // TODO

    bool isGoal( Node n ) { return false; } // TODO

    std::string showNode( Node n ) {
        std::stringstream ss;
        ss << index( n );
        return ss.str();
    }

    std::string showTransition( Node from, Node to, Label l ) {
        std::stringstream ss;
        ss << index( from ) << " -> " << index( to );
        showLabel( ss, l );
        return ss.str();
    }

    void release( Node s ) { pool().free( s ); }

    void useProperty( std::string n ) {
        assert_eq( n, "deadlock" );
    }

    template< typename Yield >
    void properties( Yield yield ) {
        yield( "deadlock", "deadlock freedom", PT_Deadlock );
    }

    ReductionSet useReductions( ReductionSet r ) override {
        return ReductionSet();
    }

    void fairnessEnabled( bool enabled ) { }

  private:
    template< typename Yield >
    void _successors( int64_t ix, Yield yield ) {
        compact.forward.map< EdgeSpec >( ix )
            ( [ yield, this ]( EdgeSpec *succs, int64_t cnt ) {
                for ( int64_t i = 0; i < cnt; ++i )
                    this->_unwrapEdge( yield, succs[ i ] );
            } );
    }

    template< typename Yield >
    auto _unwrapEdge( Yield yield, EdgeSpec es )
        -> typename std::result_of< Yield( Node, Label ) >::type
    {
        return yield( _mkNode( es.node() ), es.label() );
    }

    Node _mkNode( int64_t index ) {
        Node n = this->pool().allocate( this->slack() + sizeof( int64_t ) );
        *reinterpret_cast< int64_t * >(
                this->pool().dereference( n ) + this->slack() ) = index;
        return n;
    }

    compact::Compact compact;
};

using Compact = _Compact< wibble::Unit >;
using CompactWLabel = _Compact< uint64_t >;

}
}

#endif // DIVINE_GENERATOR_COMPACT_H

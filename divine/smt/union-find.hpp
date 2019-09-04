#include <map>
#include <iostream>

namespace divine::smt
{
    template < typename T >
    struct union_find
    {
        struct Node
        {
            T val;
            Node* parent;

            Node( T v ) : val( v ), parent( this ){}
        };

        std::map< T, Node* > nodes; // variables

        Node* make_set( T x )
        {
            auto it = nodes.find( x );
            if ( it != nodes.end() )
                return it->second;
            Node *n = new Node( x );
            nodes.insert( { x, n } );
            return n;
        }

        Node* find( Node* x )
        {
            if ( !x )
                return nullptr;
            if ( x->parent != x )
                x->parent = find( x->parent );

            return x->parent;
        }

        Node* union_( Node* x, Node* y )
        {
            if ( !x ) return y;
            if ( !y ) return x;

            auto x_root = find( x );
            auto y_root = find( y );

            if ( x_root == y_root )
                return y_root;

            x_root->parent = y_root;
            return y_root;
        }

        ~union_find()
        {
            for( auto n : nodes )
                delete n.second;
        }

        std::set< std::set< T >> get_partitions()
        {
            std::map< T, std::set< T > > parts;
            for( auto var : nodes )
            {
                auto it = parts.find( find( var.second )->val );
                if ( it != parts.end() )
                    it->second.insert( var.first );
                else
                    parts.insert( { find( var.second )->val,
                                    std::set< T >( { var.first } ) } );
            }

            std::set< std::set< T >> partitions;
            for( auto part : parts )
                partitions.insert( part.second );
            return partitions;
        }
    };
}

namespace divine::t_uf
{
    struct EvenOdd
    {
        TEST( integer )
        {
            using namespace divine::smt;

            union_find< int > uf;
            // try with nullptr
            union_find< int >::Node *even = uf.make_set( 0 ), *odd = uf.make_set( 1 );
            for( int i : { 2,4,5,7,9,6,1,0,-2,-9 } )
            {
                if ( i % 2 == 0 )
                {
                    uf.union_( even, uf.make_set( i ));
                    ASSERT_EQ( uf.find( uf.nodes.find( i )->second )->val % 2, 0 );
                }
                else
                {
                    uf.union_( odd, uf.make_set( i ));
                    ASSERT_EQ( abs( uf.find( uf.nodes.find( i )->second )->val % 2 ), 1 );
                }
            }
            auto parts = uf.get_partitions();
            assert( parts.size() == 2 );

            for( auto& part : parts )
                if( part.find( 0 ) != part.end() )
                    assert( part == std::set< int >( { 2,4,6,0,-2 } ) );
                else
                    assert( part == std::set< int >( { 5,7,9,1,-9 } ) );
        }
    };
}

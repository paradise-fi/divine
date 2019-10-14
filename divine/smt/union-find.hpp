#include <map>
#include <iostream>
#include <optional>

namespace divine::smt
{
    template < typename T >
    struct union_find
    {
        using MaybeT = std::optional< T >;

        std::map< T, T > grouping;

        T make_set( T x )
        {
            auto it = grouping.find( x );
            if ( it != grouping.end() )
                return it->second;
            grouping.insert( { x, x } );
            return x;
        }

        MaybeT find( T x )
        {
            auto it = grouping.find( x );
            if ( it != grouping.end() )
            {
                if ( it->second != x )
                    it->second = *find( it->second );
                return it->second;
            }
            return std::nullopt;
        }

        MaybeT union_( MaybeT x, MaybeT y )
        {
            if ( !x ) return y;
            if ( !y ) return x;

            auto x_root = find( *x );
            auto y_root = find( *y );

            if ( !x_root || !y_root )
                throw std::runtime_error( "representant not found in union" );

            if ( x_root == y_root )
                return y_root;

            grouping.find( *x_root )->second = *y_root;
            return y_root;
        }

        std::set< std::set< T > > get_partitions()
        {
            std::map< T, std::set< T > > parts;
            for( auto var : grouping )
            {
                auto it = parts.find( *find( var.second ) );
                if ( it != parts.end() )
                    it->second.insert( var.first );
                else
                    parts.insert( { *find( var.second ),
                                    std::set< T >( { var.first } ) } );
            }

            std::set< std::set< T > > partitions;
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
            int even = uf.make_set( 0 ), odd = uf.make_set( 1 );
            for( int i : { 2, 4, 5, 7, 9, 6, 1, 0, -2, -9 } )
            {
                if ( i % 2 == 0 )
                {
                    uf.union_( even, uf.make_set( i ) );
                    ASSERT_EQ( *uf.find( uf.grouping.find( i )->second ) % 2, 0 );
                }
                else
                {
                    uf.union_( odd, uf.make_set( i ) );
                    ASSERT_EQ( abs( *uf.find( uf.grouping.find( i )->second ) % 2 ), 1 );
                }
            }
            auto parts = uf.get_partitions();
            assert( parts.size() == 2 );

            for( auto& part : parts )
                if( part.find( 0 ) != part.end() )
                    assert( part == std::set< int >( { 2, 4, 6, 0, -2 } ) );
                else
                    assert( part == std::set< int >( { 5, 7, 9, 1, -9 } ) );
        }
    };
}

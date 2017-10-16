// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

template< typename T >
struct FieldTrie {
    using key_type = size_t;
    using Path = std::vector< key_type >;

    void insert( const Path & keys, T val ) {
        std::get< Internal >( *createPath( keys ) ).setValue( val );
    }

    std::optional< T > search( const Path & keys ) {
        auto level = root.get();
        for ( auto k : keys ) {
            if ( !level )
                return {};
            auto & i = std::get< Internal >( *level );
            if ( !i.children.count( k ) )
                return {};
            level = i.children.at( k ).get();
        }
        return std::get< Internal >( *level ).value();
    }

private:
    struct TrieNode;
    using TrieNodePtr = std::unique_ptr< TrieNode >;

    struct Leaf {
        Leaf( T v ) : value( v ) {}
        T value;
    };

    struct Internal {
        ArrayMap< key_type, TrieNodePtr > children;

        bool isLeaf() const {
            return children.size() == 1 && children.count( 0 ) &&
                   std::holds_alternative< Leaf >( *children.at( 0 ) );
        }

        void setValue( T val ) {
            if ( children.empty() )
                children[ 0 ] = TrieNode::make_leaf( val );
            assert( isLeaf() && value() == val );
        }

        T & value() const {
            assert( isLeaf() );
            return std::get< Leaf >( *children.at( 0 ) ).value;
        }

        TrieNode * getOrInsertChild( key_type key ) {
            if ( !children.count( key ) )
                children[ key ] = TrieNode::make_internal();
            return children[ key ].get();
        }
    };

    using Storage = std::variant< Leaf, Internal >;
    struct TrieNode : Storage {
        using Storage::Storage;

        static TrieNodePtr make_internal() {
            return std::make_unique< TrieNode >( Internal{} );
        }

        static TrieNodePtr make_leaf( T val ) {
            return std::make_unique< TrieNode >( Leaf{ val } );
        }
    };

    TrieNode * createPath( const Path & keys ) {
        if ( !root )
            root = TrieNode::make_internal();
        auto level = root.get();
        for ( auto k : keys ) {
            auto & i = std::get< Internal >( *level );
            level = i.getOrInsertChild( k );
        }
        return level;
    }

    TrieNodePtr root;
};

using Indices = FieldTrie< Domain >::Path;

template< typename Value >
struct ValueField {
    ValueField( Value from, Value to, Indices indices )
        : from( from ), to( to ), indices( indices ) {}

    Value from;
    Value to;
    Indices indices;
};

template< typename Value >
struct AbstractFields {
    using Field = ValueField< Value >;

    void insert( const Field & field, Domain dom ) {
        if ( fields.count( field.to ) )
            UNREACHABLE( "Dont know how to rebase indices yet." );
        if ( !fields.count( field.from ) )
            fields[ field.from ] = std::make_shared< FieldTrie< Domain > >();
        fields[ field.from ]->insert( field.indices, dom );
    }

    void alias( const Value & a, const Value & b ) {
        assert( fields.count( a.first ) );
        fields[ b.first ] = fields[ a.first ];
    }

    std::optional< Domain > getDomain( const Field & field ) {
        return fields[ field.from ]->search( field.indices );
    }

    using FieldTriePtr = std::shared_ptr< FieldTrie< Domain > >;
    std::map< Value, FieldTriePtr > fields;
};

} // namespace abstract
} // namespace lart


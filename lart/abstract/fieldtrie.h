// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

template< typename T >
struct FieldTrie {
    using key_type = size_t;
    using Path = std::vector< key_type >;

    struct TrieNode;
    using TrieNodePtr = std::unique_ptr< TrieNode >;

    struct Handle {
        Handle( FieldTrie * trie, TrieNode * root ) : trie( trie ), root( root ) {}

        Handle( const Handle & ) = default;
        Handle( Handle && ) = default;
        Handle& operator=( const Handle & ) = default;
        Handle& operator=( Handle && ) = default;

        std::optional< T > search( const Path & keys ) {
            assert( root );
            auto level = root;
            for ( auto k : keys ) {
                if ( !level )
                    return {};
                auto & i = std::get< Internal >( *level );
                if ( !i.children.count( k ) )
                    return {};
                level = i.children.at( k ).get();
            }
            if ( std::get< Internal >( *level ).isLeaf() )
                return std::get< Internal >( *level ).value();
            return std::nullopt;
        }

        Handle insert( const Path & keys, T val ) {
            std::get< Internal >( *( trie->createPath( keys, root ) ) ).setValue( val );
            if ( !root ) {
                assert( trie->_root );
                root = trie->_root.get();
            }
            return { trie, root };
        }

        Handle rebase( const Path & keys ) {
            assert( trie->_root );
            assert( root = trie->_root.get() );
            auto r = trie->rebase( keys );
            return { trie, r };
        }

    private:
        FieldTrie * trie;
        TrieNode * root;
    };


    Handle root() { return Handle( this, _root.get() ); }

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

    TrieNode * createPath( const Path & keys, TrieNode * handle ) {
        if ( !handle ) {
            assert( _root == nullptr );
            _root = TrieNode::make_internal();
            handle = _root.get();
        }
        auto level = handle;
        for ( auto k : keys ) {
            auto & i = std::get< Internal >( *level );
            level = i.getOrInsertChild( k );
        }
        return level;
    }

    TrieNode * rebase( const Path & keys ) {
        for ( auto k = keys.rbegin(); k != keys.rend(); ++k ) {
            auto level = TrieNode::make_internal();
            std::get< Internal >( *level ).children[ *k ] = std::move( _root );
            _root = std::move( level );
        }
        return _root.get();
    }

    TrieNodePtr _root;
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
        if ( fields.count( field.to ) ) {
            if ( fields.count( field.from ) )
                return;
            auto handle = fields.at( field.to ).rebase( field.indices );
            fields.insert( { field.from, handle } );
        } else {
            if ( !fields.count( field.from ) ) {
                auto& t = tries.emplace_back( std::make_unique< FieldTrie< Domain > >() );
                fields.insert( { field.from, t->root() } );
            }
            fields.at( field.from ).insert( field.indices, dom );
        }
    }

    void alias( const Value & a, const Value & b ) {
        assert( fields.count( a.first ) );
        fields[ b.first ] = fields[ a.first ];
    }

    std::optional< Domain > getDomain( const Field & field ) {
        return fields.at( field.from ).search( field.indices );
    }

    using FieldTriePtr = std::unique_ptr< FieldTrie< Domain > >;
    std::map< Value, FieldTrie< Domain >::Handle > fields;
    std::vector< FieldTriePtr > tries;
};

} // namespace abstract
} // namespace lart


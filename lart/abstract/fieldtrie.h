// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/util.h>

#include <brick-types>

namespace lart {
namespace abstract {

struct LoadStep : brick::types::Unit, brick::types::Eq {};

using GEPStep = size_t;
using Step = std::variant< GEPStep, LoadStep >;

using Path = std::vector< Step >;

template< typename K, typename T >
struct FieldTrie {

    struct TrieNode;
    using TrieNodePtr = std::unique_ptr< TrieNode >;

    struct Leaf {
        Leaf( T v ) : value( v ) {}
        T value;
    };

    struct Internal {
        ArrayMap< K, TrieNodePtr > children;

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

        TrieNode * getOrInsertChild( const K & key ) {
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

    struct Handle {
        Handle( FieldTrie * trie, TrieNode * root ) : trie( trie ), root( root ) {}

        Handle( const Handle & ) = default;
        Handle( Handle && ) = default;
        Handle& operator=( const Handle & ) = default;
        Handle& operator=( Handle && ) = default;

        Handle search( const Path & keys ) {
            assert( root );
            auto level = root;
            for ( auto k : keys ) {
                if ( !level )
                    return { trie, nullptr };
                auto & i = std::get< Internal >( *level );
                if ( !i.children.count( k ) )
                    return { nullptr, nullptr };
                level = i.children.at( k ).get();
            }
            return { trie, level };
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

        TrieNode * getRoot() const {
            return root;
        }
    private:
        FieldTrie * trie;
        TrieNode * root;
    };

    Handle root() { return Handle( this, _root.get() ); }

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

template< typename Value >
struct ValueField {
    ValueField( Value from, Value to, Path indices )
        : from( from ), to( to ), indices( indices ) {}

    Value from;
    Value to;
    Path indices;
};

template< typename Value >
struct AbstractFields {
    using Field = ValueField< Value >;
    using Trie = FieldTrie< Step, Domain >;

    void insert( const Field & field, Domain dom ) {
        if ( fields.count( field.to ) ) {
            if ( fields.count( field.from ) )
                return;
            auto handle = fields.at( field.to ).rebase( field.indices );
            fields.insert( { field.from, handle } );
        } else {
            if ( !fields.count( field.from ) ) {
                auto& t = tries.emplace_back( std::make_unique< Trie >() );
                fields.insert( { field.from, t->root() } );
            }
            fields.at( field.from ).insert( field.indices, dom );
        }
    }

    void alias( const Value & a, const Value & b ) {
        assert( fields.count( a ) );
        auto handle = fields.at( a );
        fields.insert( { b, handle } );
    }

    std::optional< Domain > getDomain( const Field & field ) {
        auto handle = fields.at( field.from ).search( field.indices );
        if ( handle.getRoot() ) {
            auto& node = std::get< Trie::Internal >( *handle.getRoot() );
            if ( node.isLeaf() )
                return node.value();
            fields.insert( { field.to, handle } );
        }
        return std::nullopt;
    }

    using FieldTriePtr = std::unique_ptr< Trie >;
    std::map< Value, Trie::Handle > fields;
    std::vector< FieldTriePtr > tries;
};

} // namespace abstract
} // namespace lart


// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/util.h>
#include <brick-types>

#include <fstream>

namespace lart {
namespace abstract {

struct LoadStep : brick::types::Unit {};

using GEPStep = size_t;
using Step = std::variant< GEPStep, LoadStep >;

using Indices = std::vector< Step >;

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

        Handle search( const Indices & keys ) {
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

        void setValue( T val ) {
            auto in = std::get_if< Internal >( root );
            assert( in && in->children.empty() );
            in->setValue( val );
        }

        using FromToHandles = std::pair< Handle, Handle >;
        FromToHandles createPath( const Indices & keys ) {
            auto node = trie->createPath( keys, root );
            if ( !root ) {
                assert( trie->_root );
                root = trie->_root.get();
            }
            return { { trie, root }, { trie, node } };
        }

        Handle rebase( const Indices & keys ) {
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

    TrieNode * createPath( const Indices & keys, TrieNode * handle ) {
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

    TrieNode * rebase( const Indices & keys ) {
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
struct AbstractFields {

    struct Path {
        Path( Value from, Value to, Indices indices )
            : from( from ), to( to ), indices( indices ) {}
        Path( Value from, Indices indices )
            : Path( from, nullptr, indices ) {}

        Value from;
        Value to;
        Indices indices;
    };

    using Trie = FieldTrie< Step, Domain >;

    void create( const Value & root ) {
        assert( root );
        if ( !fields.count( root ) ) {
            auto& t = tries.emplace_back( std::make_unique< Trie >() );
            fields.insert( { root, t->root() } );
            // create load edge
            fields.at( root ).createPath( { LoadStep{} } );
        }
    }

    void addPath( const Path & path ) {
        assert( fields.count( path.from ) );
        auto handle = fields.at( path.from ).createPath( path.indices );
        if ( path.to )
            fields.insert( { path.to, handle.second } );
    }

    void insert( const Path & path ) {
        if ( fields.count( path.to ) ) {
            if ( fields.count( path.from ) )
                return;
            auto handle = fields.at( path.to ).rebase( path.indices );
            fields.insert( { path.from, handle } );
        } else {
            if ( !fields.count( path.from ) ) {
                auto& t = tries.emplace_back( std::make_unique< Trie >() );
                fields.insert( { path.from, t->root() } );
            }
            auto ft = fields.at( path.from ).createPath( path.indices );
            fields.insert( { path.from, ft.first } );
            fields.insert( { path.to, ft.second } );
        }
    }

    void setDomain( const Path & path, Domain dom ) {
        addPath( path );
        auto handle = fields.at( path.from ).search( path.indices );
        assert( handle.getRoot() );
        handle.setValue( dom );
    }

    void setDomain( const Value & val, Domain dom ) {
        assert( fields.count( val ) );
        assert( val->getType()->isIntegerTy() );
        setDomain( { val, val, {} }, dom );
    }

    void alias( const Value & a, const Value & b ) {
        assert( fields.count( a ) );
        auto handle = fields.at( a );
        fields.insert( { b, handle } );
    }

    bool has( const Value & v ) { return fields.count( v ); }

    std::optional< Domain > getDomain( const Path & path ) {
        if ( fields.count( path.from ) ) {
            auto handle = fields.at( path.from ).search( path.indices );
            return getDomain( handle );
        }
        return std::nullopt;
    }

    std::optional< Domain > getDomain( const Value & val ) {
        if ( fields.count( val ) )
            return getDomain( fields.at( val ) );
        return std::nullopt;
    }

private:
    std::optional< Domain > getDomain( const Trie::Handle & handle ) {
        if ( handle.getRoot() ) {
            auto& node = std::get< Trie::Internal >( *handle.getRoot() );
            if ( node.isLeaf() )
                return node.value();
        }
        return std::nullopt;
    }

    using FieldTriePtr = std::unique_ptr< Trie >;
    std::map< Value, Trie::Handle > fields;
    std::vector< FieldTriePtr > tries;
};

} // namespace abstract
} // namespace lart


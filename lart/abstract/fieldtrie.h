// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once
DIVINE_RELAX_WARNINGS
#include <llvm/Support/raw_os_ostream.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <brick-types>

#include <fstream>
#include <sstream>

namespace lart {
namespace abstract {

struct LoadStep : brick::types::Unit {};

using GEPStep = size_t;
using Step = brick::types::Union< GEPStep, LoadStep >;

inline std::ostream& operator<<(std::ostream& os, const Step& step) {
    if ( auto label = step.template asptr< GEPStep >() )
        os << *label;
    else
        os << "Load";
    return os;
}

using Indices = std::vector< Step >;

template< typename K, typename T >
struct FieldTrie {

    struct TrieNode;

    using BackEdge = TrieNode *;
    using ForwardEdge = std::unique_ptr< TrieNode >;
    using EdgeStorage = brick::types::Union< ForwardEdge, BackEdge >;

    struct Edge : EdgeStorage {
        using EdgeStorage::EdgeStorage;
        using pointer = TrieNode *;

        Edge() : EdgeStorage( ForwardEdge() ) {}
        explicit operator bool() const { return get(); }
        pointer operator->() const { return get(); }

        pointer get() const {
            if ( auto fwd = EdgeStorage::template asptr< ForwardEdge >() )
                return fwd->get();
            else
                return EdgeStorage::template get< BackEdge >();
        }
    };

    static bool isBackEdge( const Edge & e ) {
        return e.template is< BackEdge >();
    }

    using TrieNodePtr = Edge;
    using Useless = std::set< TrieNode * >;

    struct TrieNode {
        using Children = ArrayMap< K, Edge >;

        TrieNode( TrieNode * parent ) : _parent( parent ) {}

        static TrieNodePtr make_node( TrieNode * parent = nullptr ) {
            return std::make_unique< TrieNode >( parent );
        }

        bool isLeaf() const { return _data.has_value(); }

        void setValue( T val ) {
            assert( !_data && _children.empty() );
            _data = val;
        }

        T const& value() const {
            assert( isLeaf() );
            return _data.value();
        }

        template< class... Args >
        void emplaceChild( Args&&... args ) {
            _children.emplace( std::forward< Args >( args )... );
        }

        TrieNode * getOrInsertChild( const K & key ) {
            if ( !_children.count( key ) )
                emplaceChild( key, make_node( this ) );
            return _children[ key ].get();
        }

        void eraseChild( const K & key ) {
            assert( _children.at( key )->children().empty() );
            _children.erase( key );
        }

        bool reachable( TrieNode * node ) {
            auto reachableIn = [&] ( const Edge & e ) {
                if ( e.get() == node )
                    return true;
                if ( isBackEdge( e ) )
                    return false;
                return e->reachable( node );
            };

            for ( const auto & ch : _children )
                if ( reachableIn( ch.second ) )
                    return true;
            return false;
        }

        TrieNode * computeBackEdge( const TrieNode * a, const TrieNode * b, const TrieNode * p ) const {
            while ( a->parent() && b->parent() ) {
                if ( a->parent() == p )
                    return b->parent();
                a = a->parent();
                b = b->parent();
            }
            return nullptr;
        }

        TrieNodePtr copy( TrieNode * parent ) const {
            auto node = make_node( parent );
            if ( isLeaf() ) {
                node->setValue( _data.value() );
            } else {
                for ( const auto & ch : _children ) {
                    if ( isBackEdge( ch.second ) ) {
                        auto back = computeBackEdge( this, node.get(), ch.second.get() );
                        assert( back && "BackEdge is not reachable in copy." );
                        node->emplaceChild( ch.first, back );
                    } else {
                        auto childCopy = ch.second->copy( node.get() );
                        node->emplaceChild( ch.first, std::move( childCopy ) );
                    }
                }
            }
            return node;
        }

        void draw( std::ofstream & os ) const {
            if ( isLeaf() ) {
                os << '"' << this << "\"[label=\"" << _data.value() << "\"];\n";
            } else {
                os << '"' << this << "\";\n";
                for ( auto & child : _children ) {
                    if ( !isBackEdge( child.second ) )
                        child.second->draw( os );
                    os << '"' << this
                       << "\" -> \""
                       << child.second.get() << '"';
                    os << "[label = \"" << child.first << "\"];\n";
                }
            }
        }

        bool useless( Useless & nodes ) {
            if ( isLeaf() )
                return false;

            bool hasNoLeaf = true;
            for ( auto & child : _children )
                if ( !isBackEdge( child.second ) )
                    hasNoLeaf &= child.second->useless( nodes );
            if ( hasNoLeaf )
                nodes.insert( this );
            return hasNoLeaf;
        }

        void setParent( TrieNode * parent ) { _parent = parent; }
        TrieNode * parent() const { return _parent; }
        Children const& children() const { return _children; }
    private:
        TrieNode * _parent;
        std::optional< T > _data;
        Children _children;
    };

    struct Handle {
        Handle( FieldTrie * trie, TrieNode * ptr ) : _trie( trie ), _ptr( ptr ) {}

        Handle( const Handle & ) = default;
        Handle( Handle && ) = default;
        Handle& operator=( const Handle & ) = default;
        Handle& operator=( Handle && ) = default;

        explicit operator bool() const { return _ptr && _trie; }

        Handle search( const Indices & keys ) const {
            assert( _ptr );
            auto level = _ptr;
            for ( auto k : keys ) {
                if ( !level )
                    return { _trie, nullptr };
                auto & i = *level;
                if ( !i.children().count( k ) )
                    return { nullptr, nullptr };
                level = i.children().at( k ).get();
            }
            return { _trie, level };
        }

        void setValue( T val ) {
            assert( _ptr && _ptr->children().empty() );
            _ptr->setValue( val );
        }

        void setNode( TrieNode * node ) {
            _ptr = node;
        }

        using FromToHandles = std::pair< Handle, Handle >;
        FromToHandles createPath( const Indices & keys ) {
            auto handle = _trie->createPath( keys, _ptr );
            if ( !_ptr ) {
                assert( _trie->root() );
                _ptr = _trie->root().ptr();
            }
            return { _trie->root(), handle };
        }

        void createBackEdge( const Step & step, BackEdge && to ) {
            if ( _ptr->children().count( step ) )
                _ptr->eraseChild( step );
            _ptr->emplaceChild( step, std::move( to ) );
        }

        bool isBackEdge( const Step & step ) {
            const auto& children = _ptr->children();
            if ( children.count( step ) )
                return FieldTrie::isBackEdge( children.at( step ) );
            return false;
        }

        Handle rebase( const Indices & keys ) {
            assert( _trie->root() );

            auto rebaseroot = _ptr;
            auto k = keys.rbegin();
            size_t count = 0;
            while ( k != keys.rend() && rebaseroot->parent() ) {
                rebaseroot = rebaseroot->parent();
                ++k;
                count++;
            }

            Indices indices = { keys.begin(), keys.end() - count };

            if ( rebaseroot == _trie->root().ptr() )
                return _trie->rebase( indices );
            else
                return { _trie, rebaseroot };
        }

        bool reachable( Handle && handle ) {
            if ( _trie != handle.trie() )
                return false;
            return _ptr->reachable( handle.ptr() );
        }

        TrieNode * ptr() const { return _ptr; }
        FieldTrie * trie() const { return _trie; }
    private:
        FieldTrie * _trie;
        TrieNode * _ptr;
    };

    Handle root() { return Handle( this, _root.get() ); }

    Handle createPath( const Indices & keys, TrieNode * from ) {
        if ( !from ) {
            assert( !_root );
            _root = TrieNode::make_node();
            from = _root.get();
        }
        auto level = from;
        for ( auto k : keys )
            level = level->getOrInsertChild( k );
        return Handle( this, level );
    }

    Handle rebase( const Indices & keys ) {
        for ( auto k = keys.rbegin(); k != keys.rend(); ++k ) {
            auto level = TrieNode::make_node();
            _root->setParent( level.get() );
            level->emplaceChild( *k, std::move( _root ) );
            _root = std::move( level );
        }
        return root();
    }

    Useless useless() {
        if ( !_root )
            return {};
        Useless nodes;
        _root->useless( nodes );
        return nodes;
    }

    void draw( std::ofstream & os ) const {
        _root->draw( os );
    }

    TrieNodePtr _root;
};


template< typename Value >
struct AbstractFields {

    using CValue = ConstifyPtr< Value >;
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

    void insertBackEdge( const Path & path ) {
        assert( has( path.from ) && has( path.to ) );
        auto fh = fields.at( path.from );
        auto th = fields.at( path.to );

        assert( path.indices.size() == 1 );
        auto step = path.indices.at( 0 );

        if ( fh.isBackEdge( step ) )
            return; // back edge already exists

        if ( auto ch = fh.search( path.indices ) ) {
            for ( auto & v : fields )
                if ( v.second.ptr() == ch.ptr() )
                    v.second.setNode( th.ptr() );
        }

        fields.at( path.from ).createBackEdge( step, th.ptr() );
    }

    void setDomain( const Path & path, Domain dom ) {
        addPath( path );
        auto handle = fields.at( path.from ).search( path.indices );
        assert( handle.ptr() );
        handle.setValue( dom );
    }

    void setDomain( CValue val, Domain dom ) {
        assert( fields.count( val ) );
        assert( val->getType()->isIntegerTy() );
        setDomain( { val, val, {} }, dom );
    }

    void alias( CValue a, CValue b ) {
        assert( fields.count( a ) );
        auto handle = fields.at( a );
        fields.insert( { b, handle } );
    }

    bool has( CValue v ) const { return fields.count( v ); }

    bool inSameTrie( CValue a, CValue b ) const {
        if ( !has( a ) || !has( b ) )
            return false;
        return fields.at( a ).trie() == fields.at( b ).trie();
    }

    std::optional< Domain > getDomain( const Path & path ) const {
        if ( fields.count( path.from ) ) {
            auto handle = fields.at( path.from ).search( path.indices );
            return getDomain( handle );
        }
        return std::nullopt;
    }

    std::optional< Domain > getDomain( CValue val ) const {
        if ( fields.count( val ) )
            return getDomain( fields.at( val ) );
        return std::nullopt;
    }

    bool reachable( CValue from, CValue to ) {
        return fields.at( from ).reachable( std::move( fields.at( to ) ) );
    }

    void storeUnder( CValue from, CValue to ) {
        auto fromH = fields.at( from );
        auto toH = fields.at( to ).createPath( { LoadStep{} } ).second;
        storeUnderImpl( fromH, toH );
    }

    void draw( const std::string & path ) const {
        auto tmpPath = path + ".tmp";
        std::ofstream out( tmpPath, std::ios::out );
        out << "digraph FieldTrie {\n";

        // draw tries
        for ( const auto & trie : tries )
            trie->draw( out );

        // draw handles
        llvm::raw_os_ostream ros( out );
        for ( const auto & h : fields ) {
            ros << '"';

            std::ostringstream ss;
            llvm::raw_os_ostream rss( ss );
            rss << h.first;
            rss.flush();

            llvm::StringRef name(ss.str());

            ros.write_escaped( name );
            ros << "\" [shape=rectangle, ";
            ros << "label= \"";
            h.first->printAsOperand( ros, false );
            ros << "\"];\n";
            ros << '"';
            ros.write_escaped( name );
            ros << "\" -> \"" << h.second.ptr() << "\";\n";
        }
        ros.flush();

        out << "}" << std::endl;
        std::rename( tmpPath.c_str(), path.c_str() );
    }

    void erase( CValue v ) {
        fields.erase( v );
    }

    void clean() {
        for ( auto & trie : tries ) {
            auto useless = trie->useless();
            for ( auto it = fields.begin(); it != fields.end(); ) {
                if ( useless.count( it->second.ptr() ) )
                    it = fields.erase( it );
                else
                    ++it;
            }
        }
    }
private:
    using Handle = Trie::Handle;

    std::optional< Domain > getDomain( const Trie::Handle & handle ) const {
        if ( handle.ptr() && handle.ptr()->isLeaf() )
            return handle.ptr()->value();
        return std::nullopt;
    }

    void storeUnderImpl( Handle from, Handle to ) {
        for ( const auto & ch : from.ptr()->children() ) {
            if ( Trie::isBackEdge( ch.second ) )
                continue;
            if ( to.ptr()->children().count( ch.first ) ) {
                auto toptr = to.ptr()->children().at( ch.first ).get();
                Handle toh = { to.trie(), toptr };
                Handle fromh = { from.trie(), ch.second.get() };
                storeUnderImpl( fromh, toh );
            } else {
                auto node = ch.second->copy( to.ptr() );
                to.ptr()->emplaceChild( ch.first, std::move( node ) );
            }
        }
    }

    using FieldTriePtr = std::unique_ptr< Trie >;
    std::map< CValue , Handle > fields;
    std::vector< FieldTriePtr > tries;
};

} // namespace abstract
} // namespace lart


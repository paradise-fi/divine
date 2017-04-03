// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/Support/raw_ostream.h>
DIVINE_UNRELAX_WARNINGS

#include <utility>

#include <lart/analysis/postorder.h>

namespace lart {
namespace abstract {

    template< typename Value, typename Annotation >
    struct AnnotationNode {
        using Node = AnnotationNode< Value, Annotation >;
        using Nodes = std::vector< Node >;

        AnnotationNode() = default;
        AnnotationNode( const AnnotationNode & ) = default;
        AnnotationNode( AnnotationNode && ) = default;
        AnnotationNode( const Value val, const Annotation ann, bool root = false )
            : value( val ), annotation( ann ), root( root ) {}

        AnnotationNode& operator=( const AnnotationNode & other ) {
            AnnotationNode temp( other );
            temp.swap( *this );
            return *this;
        }

        AnnotationNode operator=( AnnotationNode && other ) {
            value = std::move( other.value );
            annotation = std::move( other.annotation );
            return *this;
        }

        void swap( AnnotationNode & other ) throw() {
            std::swap( this->value, other.value );
            std::swap( this->annotation, other.annotation );
        }

        bool operator==( const AnnotationNode & other ) const {
            return value == other.value;
        }

        Value value;
        Annotation annotation;
        bool root;
    };


    using ValueNode = AnnotationNode< llvm::Value *, std::string >;

    static std::vector< ValueNode > value_succs( const ValueNode & n ) {
        if ( llvm::isa< llvm::CallInst >( n.value ) && !n.root )
            return {};
        return query::query( n.value->users() )
            .map( [&] ( const auto & val ) { return ValueNode{ val, n.annotation, false }; } )
            .freeze();
    }
}
}

namespace std {
    template<>
    struct hash< lart::abstract::ValueNode > {
        typedef lart::abstract::ValueNode argument_type;
        typedef std::size_t result_type;

        result_type operator()( argument_type const & g ) const {
            using std::hash;
            return hash< std::string >{}( g.annotation ) ^ hash< llvm::Value * >{}( g.value );
        }
    };
}

namespace lart {
namespace abstract {
    struct FunctionNode : brick::types::Eq {
        using Nodes = std::vector< std::shared_ptr< FunctionNode > >;
        using Entries = std::unordered_set< ValueNode >;

        FunctionNode() = default;
        FunctionNode( const FunctionNode & ) = default;
        FunctionNode( FunctionNode && ) = default;
        FunctionNode( llvm::Function * f, Entries n, Nodes s = {} ) :
            function( f ), entries( n ), succs( s ) {}

        FunctionNode &operator=( FunctionNode && other ) {
            function = std::move( other.function );
            entries = std::move( other.entries );
            succs = std::move( other.succs );
            return *this;
        }

        bool operator==( const FunctionNode & other ) const {
            return function == other.function && entries == other.entries;
        }

        std::vector< ValueNode > postorder() const{
            return analysis::postorder< ValueNode >( { entries.begin(), entries.end() },
                                                     value_succs );
        }

        void dump() const {
            llvm::errs() << "\n---------------------------------\n";
            llvm::errs() << "Node: " << function->getName() << "\n";
            llvm::errs() << "Entries: \n";
            for ( const auto & e : entries ){
                e.value->dump();
            }
            llvm::errs() << "Succs: \n";
            for ( const auto & s : succs ) {
                std::cerr << s << "\n";
            }
            llvm::errs() << "\n---------------------------------\n";
        }

        llvm::Function * function;
        Entries entries;
        Nodes succs;
    };

    using FunctionNodePtr = std::shared_ptr< FunctionNode >;
    using FunctionNodes = std::vector< FunctionNodePtr >;

} // namespace abstract
} // namespace lart


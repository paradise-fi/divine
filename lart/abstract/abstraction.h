// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/builder.h>
#include <lart/abstract/data.h>
#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

namespace detail {

// Container consisting of function and its abstract roots
struct FNode {
    using Function = llvm::Function;
    using Roots = RootsSet;

    FNode( Function *fn, Roots roots, Fields *fields )
        : _fn( fn ), _roots( roots ), _fields( fields )
    {
        init();
    }

    const AbstractValues& abstract_insts() const { return _ainsts; } ;

    Function* function() const { return _fn; }
    const Roots& roots() const { return _roots; }
    Roots& roots() { return _roots; }
    Fields* fields() const { return _fields; }
    bool need_change_signature() const { return _change_sig; };

    std::optional< AbstractValue > ret() const;
    AbstractValues args() const;
    Domain return_domain() const;
    llvm::Type* return_type( TMap& ) const;
    llvm::FunctionType* function_type( TMap& ) const;

    template< typename Builder >
    void process( Builder & builder );

    void clean();

    friend bool operator<( const FNode &l, const FNode &r ) {
        using Argument = llvm::Argument;

        auto arg_hash = [] ( const FNode &fn ) -> size_t {
            size_t sum = 1;
            for ( auto &a : filterA< Argument >( fn.roots() ) ) {
                sum *= a.get< Argument >()->getArgNo();
                sum  = sum % std::numeric_limits<short>::max();
            }
            return sum;
        };

        auto ln = l.function()->getName().str();
        auto rn = r.function()->getName().str();

        return ln == rn ? arg_hash( l ) < arg_hash( r ) : ln < rn;
    }

    void init();
private:
    bool _need_change_signature() const;

    Function* _fn;
    Roots _roots;
    Fields* _fields;

    AbstractValues _ainsts;
    bool _change_sig;
};

} // detail namespace

struct Abstraction {
    using FNode = detail::FNode;

    Abstraction( PassData & data ) : data( data ) {}
    void run( llvm::Module& );

private:
    llvm::Function* create_prototype( const FNode& );
    FNode clone( const FNode& );

    PassData &data;

    using Signature = std::vector< size_t >;
    FunctionMap< llvm::Function *, Signature > fns;
};

} // namespace abstract
} // namespace lart

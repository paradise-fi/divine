// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>

#include <lart/abstract/bcp.h>
#include <lart/abstract/types.h>
#include <lart/abstract/intrinsic.h>
#include <lart/analysis/bbreach.h>
#include <lart/analysis/edge.h>
#include <lart/support/query.h>

namespace lart {
namespace abstract {
namespace {
    using BB = llvm::BasicBlock;
    using I = llvm::Instruction;

    struct Assume {
        enum AssumeValue { Predicate, LHS, RHS };

        /* Find corresponding tristate to given assume with assumed 'value'. */
        Assume( I * assume ) : assume( assume ) {}

        /* Abstract icmp condition constrained by given assume. */
        llvm::CallInst * condition() {
            auto call = llvm::cast< llvm::CallInst >( assume );
            return llvm::cast< llvm::CallInst > (
                   llvm::cast< llvm::CallInst >( call->getArgOperand( 0 ) )->getArgOperand( 0 ) );
        }

        /* Creates appropriate assume in given domain about predicate, left
           or right argument of condition.
         */
        llvm::Instruction * constrain( const std::string & domain, AssumeValue v ) {
            llvm::IRBuilder<> irb( assume );
            llvm::Type * rty;
            std::vector< llvm::Value * > args;
            switch ( v ) {
                case AssumeValue::Predicate:
                    rty = condition()->getType();
                    args.push_back( condition() );
                    break;
                case AssumeValue::LHS:
                    rty = condition()->getArgOperand( 0 )->getType();
                    args.push_back( condition()->getArgOperand( 0 ) );
                    break;
                case AssumeValue::RHS:
                    rty = condition()->getArgOperand( 1 )->getType();
                    args.push_back( condition()->getArgOperand( 1 ) );
            };
            args.push_back( condition() );

            auto call = llvm::cast< llvm::CallInst >( assume );
            args.push_back( call->getArgOperand( 1 ) );

            std::vector< llvm::Type * > arg_types;
            for ( const auto & arg : args )
                arg_types.push_back( arg->getType() );
            const std::string tag = "lart." + domain + ".assume." + types::lowerTypeName( rty );

            auto fty = llvm::FunctionType::get( rty, arg_types, false );
            auto m = irb.GetInsertBlock()->getModule();
            auto fn = m->getOrInsertFunction( tag, fty );
            return irb.CreateCall( fn , args );
        }

        llvm::Instruction * assume;
    };

    /* Replace uses of v by newv in bb. */
    void _replace( llvm::Value * v, llvm::Value * newv, BB * bb ) {
        assert( v->getType() == newv->getType() &&
                "replace of value with new value of different type!" );
        for ( auto & u : v->uses() ) {
            auto usr = llvm::dyn_cast< llvm::Instruction >( u.getUser() );
            if ( usr && usr->getParent() == bb )
                u.set( newv );
        }
    }

    /* Propagates assumed value and optionally merge with original value */
    void propagate( I * assume, I * origin ) {
        using BBEdge = analysis::BBEdge;
        using SCC = analysis::BasicBlockSCC;
        using Reachability = analysis::Reachability;

        llvm::Function * fn = assume->getParent()->getParent();
        SCC sccs( *fn );
        Reachability reach( *fn, &sccs );

        BB * abb = assume->getParent();
        BB * obb = origin->getParent();

        for ( auto user : origin->users() ) {
            if ( ( user != assume ) && ( user != origin ) ) {
                BB * ubb = llvm::cast< I >( user )->getParent();
                if ( reach.reachable( abb, ubb ) ) {
                    bool merge = false;
                    if ( !ubb->getUniquePredecessor() ) {
                        BBEdge e{ abb, abb->getUniqueSuccessor() };
                        // is obb backward reachable from ubb without going through abb?
                        e.hide();
                        SCC scc( *fn );
                        Reachability r( *fn, &scc );
                        merge = r.reachable( obb, ubb );
                        assert( !r.reachable( abb, ubb ) );
                        e.show();
                    }
                    if ( merge ) {
                        //FIXME create PHI only in the highest BB
                        llvm::IRBuilder<> irb( ubb );
                        auto phi = irb.CreatePHI( origin->getType(), 2 );
                        phi->addIncoming( origin, obb );
                        phi->addIncoming( assume, abb );
                        _replace( origin, phi, ubb );
                    } else {
                        _replace( origin, assume, ubb );
                    }
                }
            }
        }
    }
}
    void BCP::process( llvm::Module & m ) {
        auto assumes = query::query( m ).flatten().flatten()
            .map( query::refToPtr )
            .map( query::llvmdyncast< llvm::CallInst > )
            .filter( query::notnull )
            .filter( []( llvm::CallInst * call ) {
                return intrinsic::isAssume( call );
            } )
            .freeze();
        for ( auto ass : assumes )
            process( ass );
    }

    void BCP::process( llvm::Instruction * assume ) {
        Assume ass = { assume };
        const std::string domain = intrinsic::domain( ass.condition() );

        // create constraints on arguments from condition, that created tristate
        auto pre = ass.constrain( domain, Assume::AssumeValue::Predicate );
        auto lhs = ass.constrain( domain, Assume::AssumeValue::LHS );
        auto rhs = ass.constrain( domain, Assume::AssumeValue::RHS );

        // forward propagate constrained values
        propagate( pre, ass.condition() );
        propagate( lhs, llvm::cast< I >( ass.condition()->getArgOperand( 0 ) ) );
        propagate( rhs, llvm::cast< I >( ass.condition()->getArgOperand( 1 ) ) );

        assume->eraseFromParent();
    }

} // namespace abstract
} // namespace lart

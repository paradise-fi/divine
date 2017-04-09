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
    using V = llvm::Value;
    using A = llvm::Argument;

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

    using BBEdge = analysis::BBEdge;
    using SCC = analysis::BasicBlockSCC;
    using Reachability = analysis::Reachability;

    auto reachable( V * origin, I * constrain ) {
        llvm::Function * fn = constrain->getParent()->getParent();
        SCC sccs( *fn );
        Reachability reach( *fn, &sccs );

        BB * abb = constrain->getParent();

        return query::query( origin->users() )
              .map( query::llvmdyncast< I > )
              .filter( [&]( I * user ) {
                    // skip block with constrains
                  return user->getParent() != abb;
               } )
              .filter( [&]( I * user ) {
                  BB * ubb = user->getParent();
                  return reach.strictlyReachable( abb, ubb );
               } ).freeze();
    }

    bool needToMerge( I * constrain, I * user ) {
        llvm::Function * fn = constrain->getParent()->getParent();

        V * origin = constrain->getOperand( 0 );
        BB * obb = llvm::isa< A >( origin )
                 ? &llvm::cast< A >( origin )->getParent()->front()
                 : llvm::cast< I >( origin )->getParent();

        BB * ubb = user->getParent();
        BB * cbb = constrain->getParent();

        if ( ubb->getUniquePredecessor() )
            return false;

        bool needtomerge;
        assert( cbb->getUniqueSuccessor() );
        BBEdge e{ cbb, cbb->getUniqueSuccessor() };

        // is vbb reachable from obb without going through cbb?
        e.hide();
        SCC scc( *fn );
        Reachability reach( *fn, &scc );
        needtomerge = reach.reachable( obb, ubb );
        assert( !reach.reachable( cbb, ubb ) );
        e.show();

        return needtomerge;
    }

    /* Replace uses of v by newv in bb. */
    void replace( llvm::Value * v, llvm::Value * newv, BB * bb ) {
        assert( v->getType() == newv->getType() &&
                "Trying to replace a value with a value of different type!" );
        for ( auto u : v->users() ) {
            auto usr = llvm::dyn_cast< I >( u );
            if ( usr && usr->getParent() == bb ) {
                u->replaceUsesOfWith( v, newv );
            }
        }
    }

    I * dominatedByMerged( std::vector< I * > merged, BB * bb ) {
        llvm::Function * fn = bb->getParent();
        SCC scc( *fn );
        Reachability reach( *fn, &scc );

        for ( auto & m : merged )
            if ( reach.reachable( m->getParent(), bb ) )
                return m;

        // bb is not dominated by merged values
        return nullptr;
    }

    /* Propagates constrained value and optionally merge with original value */
    void propagate( I * constrain ) {
        // original value, that is constrained by assume
        V * origin = constrain->getOperand( 0 );
        BB * obb = llvm::isa< A >( origin )
                 ? &llvm::cast< A >( origin )->getParent()->front()
                 : llvm::cast< I >( origin )->getParent();

        BB * cbb = constrain->getParent();

        std::vector< I * > merged;
        if ( auto i = llvm::dyn_cast< I >( origin ) )
            merged.push_back( i );
        // We need to replace users of original value
        // that are reachable from constrain.
        for ( I * user : reachable( origin, constrain ) ) {
            BB * ubb = user->getParent();
            if ( auto dom = dominatedByMerged( merged, ubb ) ) {
                //TODO maybe need to merge dom and c
                replace( origin, dom, ubb );
            } else if ( needToMerge( constrain, user ) ) {
                llvm::IRBuilder<> irb( &ubb->front() );
                auto phi = irb.CreatePHI( origin->getType(), 2 );
                phi->addIncoming( origin, obb );
                phi->addIncoming( constrain, cbb );
                replace( origin, phi, ubb );
                merged.push_back( phi );
            } else {
                replace( origin, constrain, ubb );
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
        auto lhs = ass.constrain( domain, Assume::AssumeValue::LHS );
        auto rhs = ass.constrain( domain, Assume::AssumeValue::RHS );
        auto pre = ass.constrain( domain, Assume::AssumeValue::Predicate );

        // forward propagate constrained values
        propagate( lhs );
        propagate( rhs );
        propagate( pre );

        assume->eraseFromParent();
    }

} // namespace abstract
} // namespace lart

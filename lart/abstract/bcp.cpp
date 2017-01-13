// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>

#include <lart/abstract/bcp.h>
#include <lart/abstract/types.h>
#include <lart/abstract/intrinsic.h>
#include <lart/support/query.h>

namespace lart {
namespace abstract {
namespace {
    struct Assume {
        enum AssumeValue { Predicate, LHS, RHS };

        /* Find corresponding tristate to given assume with assumed 'value'. */
        Assume( llvm::Instruction * assume ) : assume( assume ) {}

        /* Abstract icmp condition constrained by given assume. */
        llvm::CallInst * condition() {
            auto call = llvm::cast< llvm::CallInst >( assume );
            return llvm::cast< llvm::CallInst > (
                   llvm::cast< llvm::CallInst >( call->getArgOperand( 0 ) )->getArgOperand( 0 ) );
        }

        /* Creates appropriate assume in given domain about predicate, left
           or right argument of condition.
         */
        llvm::Value * constrain( const std::string & domain, AssumeValue v ) {
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

    /* Propagates assumed value and optionaly merge with original value */
    void propagate( llvm::Value * /* assume */, llvm::Value * /* origin */ ) {
        // for  use : uses( origin ) do
        //  if ( bb( use ) is reachable from bb( assume )
        //      if ( bb( origin ) is reachable from bb( use ) tak, ze neprejde, cez bb( assume )
        //          create phi [origin = origin | assume ]
        //      else
        //          substituuj a za assume
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
        propagate( lhs, ass.condition()->getArgOperand( 0 ) );
        propagate( rhs, ass.condition()->getArgOperand( 1 ) );

        assume->eraseFromParent();
    }

} // namespace abstract
} // namespace lart

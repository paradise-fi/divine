#include <lart/abstract/substitution.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <lart/support/query.h>
#include <lart/support/util.h>
#include <lart/abstract/metadata.h>

#include <lart/abstract/domains/domains.h>
#include <lart/abstract/domains/tristate.h>
#include <lart/abstract/domains/zero.h>
#include <lart/abstract/domains/sym.h>


#include <iostream>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {

Functions lifters( Module &m ) {
    return query::query( m )
        .map( query::refToPtr )
        .filter( [] ( auto fn ) {
            return fn->getName().startswith( "lart.gen" );
        } )
        .freeze();
}

decltype(auto) abstract_insts( Function &fn ) {
    return query::query( fn ).flatten()
        .map( query::refToPtr )
        .filter( [] ( auto v ) { return is_one_of< CallInst, PHINode >( v ); } )
        .freeze();
}

Values in_domain_args( CallInst *intr, std::map< Value*, Value* > &smap ) {
    return query::query( intr->arg_operands() )
        .map( [&] ( auto &arg ) { return arg.get(); } )
        .map( [&] ( auto arg ) { return smap.count( arg ) ? smap[ arg ] : arg; } )
        .freeze();
}

} // anonymous namespace

void Substitution::run( Module &m ) {
    init_domains( m );
    for ( auto &lift : lifters( m ) ) {
        std::map< Value*, Value* > smap;

        auto ainsts = abstract_insts( *lift );
        for ( auto &i : ainsts ) {
            if ( auto intr = dyn_cast< CallInst >( i ) ) {
                auto args = in_domain_args( intr, smap );
                smap[ intr ] = process( intr, args );
            }

            if ( auto phi = dyn_cast< PHINode >( i ) ) {
                IRBuilder<> irb( phi );
                auto ty = smap[ phi->getIncomingValue( 0 ) ]->getType();
                auto aphi = irb.CreatePHI( ty, phi->getNumIncomingValues() );

                for ( size_t i = 0; i < phi->getNumIncomingValues(); ++i ) {
                    auto val = smap[ phi->getIncomingValue( i ) ];
                    auto bb = cast< Instruction >( val )->getParent();
                    aphi->addIncoming( val, bb );
                }

                smap[ phi ] = aphi;
            }
        }

        for ( auto &i : lart::util::reverse( ainsts ) )
            i->eraseFromParent();
    }

}


Value* Substitution::process( CallInst *intr, Values &args ) {
    auto dom = is_lift( intr )
             ? MDValue( intr ).domain()
             : MDValue( intr->getOperand( 0 ) ).domain();

    assert( domains.count( dom ) && "Unknown domain for substitution" );
    return domains[ dom ]->process( intr, args );
}

void Substitution::add_domain( std::shared_ptr< Common > dom ) {
    domains[ dom->domain() ] = dom;
}

void Substitution::init_domains( Module & ) {
    add_domain( std::make_shared< Tristate >() );
    add_domain( std::make_shared< Symbolic >() );
    add_domain( std::make_shared< Zero >() );
}

} // namespace abstract
} // namespace lart

// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/stores.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>

using namespace llvm;

namespace lart {
namespace abstract {

Value * offset( Value * ptr ) {
    // TODO get offset from index placeholder

    if ( auto gep = dyn_cast< GetElementPtrInst >( ptr ) ) {
        assert( gep->getNumIndices() == 1 );
        return gep->idx_begin()->get();
    }

    UNREACHABLE( "Unsupported offset for abstract store." );
}

template< typename T >
std::vector< T * > transformable( Module & m ) {
    return query::query( abstract_metadata( m ) )
        .map( [] ( auto mdv ) { return mdv.value(); } )
        .map( query::llvmdyncast< T > )
        .filter( query::notnull )
        .filter( is_transformable )
        .freeze();
}

void AddStores::run( Module &m ) {
    for ( const auto &gep : transformable< GetElementPtrInst >( m ) ) {
        assert( gep->getNumIndices() == 1 );
        set_addr_offset( gep, gep->idx_begin()->get() );
        set_addr_origin( gep, gep->getPointerOperand() );
        // TODO propaget geps through phis + propaget to functions
    }

    for ( const auto &store : transformable< StoreInst >( m ) )
        process( store );
}

Function * abstract_store( Module * m, StoreInst * store ) {
    // return some arbitrary type - need to acth taint call
    auto rty = Type::getInt1Ty( store->getContext() );

    auto val = store->getValueOperand();
    auto ptr = store->getPointerOperand();

    auto fty = llvm::FunctionType::get( rty, { val->getType(), ptr->getType() }, false );
    auto name = "lart.store.placeholder." + llvm_name( val->getType() );
    return lart::util::get_or_insert_function( m, fty, name );
}

void AddStores::process( StoreInst * store ) {
    auto val = store->getValueOperand();
    auto ptr = store->getPointerOperand();

    auto fn = abstract_store( util::get_module( store ), store );

    IRBuilder<> irb( store );
    auto ph = irb.CreateCall( fn, { val, ptr } );

    add_abstract_metadata( ph, get_domain( store ) );
    make_duals( store, ph );
}

} // namespace abstract
} // namespace lart

// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/content.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>

using llvm::Module;
using llvm::Function;
using llvm::Type;
using llvm::LoadInst;
using llvm::StoreInst;
using llvm::GetElementPtrInst;
using llvm::IRBuilder;

namespace lart {
namespace abstract {

namespace {

Function * abstract_store( Module * m, StoreInst * store ) {
    // return some arbitrary type - needed for taint call
    auto rty = Type::getInt1Ty( store->getContext() );

    auto val = store->getValueOperand();
    auto ptr = store->getPointerOperand();

    auto fty = llvm::FunctionType::get( rty, { val->getType(), ptr->getType() }, false );
    auto name = "lart.store.placeholder." + llvm_name( val->getType() );
    return lart::util::get_or_insert_function( m, fty, name );
}

Function * abstract_load( Module * m, LoadInst * load ) {
    auto rty = load->getType();
    auto ptr = load->getPointerOperand();

    auto fty = llvm::FunctionType::get( rty, { load->getType(), ptr->getType() }, false );
    auto name = "lart.load.placeholder." + llvm_name( rty );
    return lart::util::get_or_insert_function( m, fty, name );
}

} // anonymous namespace

void StoresToContent::run( Module &m ) {
    for ( const auto &store : transformable< StoreInst >( m ) ) {
        if ( Domain::get( store->getValueOperand() ) != Domain::get( store ) ) {
            process( store );
        }
    }
}

void StoresToContent::process( StoreInst * store ) {
    auto val = store->getValueOperand();
    auto ptr = store->getPointerOperand();

    auto fn = abstract_store( store->getModule(), store );

    IRBuilder<> irb( store );
    auto ph = irb.CreateCall( fn, { val, ptr } );

    add_abstract_metadata( ph, Domain::get( store ) );
    meta::make_duals( store, ph );
    ph->moveAfter( store );
}

void LoadsFromContent::run( Module & m ) {
    for ( const auto &load : transformable< LoadInst >( m ) ) {
        if ( Domain::get( load->getPointerOperand() ) != Domain::get( load ) ) {
            process( load );
        }
    }
}

void LoadsFromContent::process( LoadInst * load ) {
    auto ptr = load->getPointerOperand();
    auto fn = abstract_load( load->getModule(), load );

    IRBuilder<> irb( load );
    auto ph = llvm::cast< llvm::CallInst >( irb.CreateCall( fn, { load, ptr } ) );
    add_abstract_metadata( ph, Domain::get( load ) );
    meta::make_duals( load, ph );

    load->replaceAllUsesWith( ph );
    ph->moveAfter( load );
}


} // namespace abstract
} // namespace lart

// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/interrupt.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>

using namespace lart::abstract;
using namespace llvm;

namespace {

Function * interrupt_function( Module &m ) {
    auto void_ty = Type::getVoidTy( m.getContext() );
    auto fty = FunctionType::get( void_ty, false );
    auto interrupt = get_or_insert_function( &m, fty, "__dios_suspend" );
    interrupt->addFnAttr( Attribute::NoUnwind );
    return interrupt;
}

} // anonymous namespace

void CallInterrupt::run( llvm::Module& m ) {
    auto interrupt = interrupt_function( m );
    for ( auto &fn : m ) {
        if ( fn.getMetadata( "lart.abstract.roots" ) ) {
            auto entry = fn.getEntryBlock().getFirstInsertionPt();
            IRBuilder<>( entry ).CreateCall( interrupt );
        }
    }
}


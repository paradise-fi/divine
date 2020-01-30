/*
 * (c) 2020 Lukáš Korenčik <xkorenc1@fi.muni.cz>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-llvm>

#include <lart/abstract/util.h>
#include <lart/support/meta.h>
#include <lart/support/query.h>
#include <lart/support/util.h>

namespace lart::mcsema
{

    struct lower_ret_agg : abstract::LLVMUtil< lower_ret_agg >
    {
        using values_t = std::vector< llvm::Value * >;

        using functions_t = std::vector< llvm::Function * >;
        using functions_map_t = std::unordered_map< llvm::Function *, llvm::Function * >;
        using agg_to_wrapper_t = std::unordered_map< llvm::Function *, llvm::Function * >;
        using types_t = std::vector< llvm::Type * >;

        constexpr static const char *wrapper_prefix = "divine.ret.wrapper";

        llvm::Module *_m;
        const llvm::DataLayout &_dl;
        llvm::LLVMContext &context;

        bool is_lifted( llvm::Function &f )
        {
            auto md = f.getMetadata( "remill.function.type" );
            if ( !md )
                return false;
            if ( md->getNumOperands() != 1 )
                UNREACHABLE( "Lifted bc has invalid remill.function.type annotation");

            auto md_str = llvm::dyn_cast< llvm::MDString >( md->getOperand( 0 ) );

            if ( !md_str )
                UNREACHABLE( "remill.function.type annotation has incorrect op type");

            return md_str->getString().contains( "lifted" ) ||
                   md_str->getString().contains( "helper.mcsema" );
        }

        functions_t lifted_funcs()
        {
            functions_t out;
            for ( auto &f : *_m )
                if ( is_lifted( f ) )
                    out.push_back( &f );
            return out;
        }

        auto wrap_ret_t( llvm::Function *f ) { return ptr( f->getReturnType() ); }

        functions_map_t change_ret_type()
        {
            functions_map_t changed;
            for ( auto f : lifted_funcs() )
                changed.insert( { f, changeReturnType( f, wrap_ret_t( f ) ) } );
            return changed;
        }


        void flatten( llvm::Type *type, types_t &result )
        {
            if ( util::is_one_of_types< llvm::PointerType, llvm::IntegerType >( type ) )
            {
                result.push_back( type );
                return;
            }

            if ( auto struct_t = llvm::dyn_cast< llvm::StructType >( type ) )
            {
                for ( auto e : struct_t->elements() )
                    flatten( e, result );
                return;
            }
            UNREACHABLE( "Cannot flatten this type" );
        }

        std::string next_name()
        {
            static uint64_t counter = 0;
            return std::string( wrapper_prefix ) + std::to_string( ++counter );
        }

        // FIXME: Currently we leak memory
        template< typename irb_t >
        void free( llvm::Value * val, irb_t &irb )
        {
            UNREACHABLE( "Not implemented" );
        }

        template< typename irb_t >
        llvm::Value *allocate( llvm::Type * type, irb_t &irb )
        {
            auto allocator_f = _m->getFunction( "__vm_obj_make" );
            if ( !allocator_f )
                UNREACHABLE( "Could not find allocator while lowering struct" );

            auto memory = irb.CreateCall(
                    allocator_f,
                    { i32( _dl.getTypeAllocSize( type ) ), i32( 3 ) } );
            return irb.CreateBitCast( memory, ptr( type ) );
        }

        llvm::Function *synthetize_wrapper( llvm::Type * type )
        {

            auto struct_t = llvm::dyn_cast< llvm::StructType >( type );
            ASSERT( struct_t );

            types_t flattened;
            flatten( struct_t, flattened );

            auto wrapper_t = llvm::FunctionType::get( ptr( type ), flattened, false );
            auto wrapper_fc = _m->getOrInsertFunction( next_name(), wrapper_t );
            auto wrapper_f = llvm::dyn_cast< llvm::Function >( wrapper_fc );

            auto entry = llvm::BasicBlock::Create( context, "entry" , wrapper_f );

            llvm::IRBuilder<> irb( entry );
            auto memory = allocate( type, irb );

            for ( auto i = 0U; i < struct_t->getNumElements(); ++i )
            {
                auto gep = irb.CreateInBoundsGEP( memory, { i32( 0 ), i32( i ) } );
                irb.CreateStore( argument( wrapper_f, i ), gep );
            }
            irb.CreateRet( memory );
            return wrapper_f;
        }

        agg_to_wrapper_t synthetize_wrappers()
        {
            agg_to_wrapper_t wrappers;
            for ( auto f : lifted_funcs() )
            {
                auto ret_t = f->getReturnType();
                // Since there can be already clonned functions with ptr as return type
                if ( !ret_t->isPointerTy() )
                    continue;

                auto struct_type =
                    llvm::dyn_cast< llvm::PointerType >( ret_t )->getElementType();
                auto &wrapper = wrappers[ f ];
                if ( !wrapper )
                {
                    wrapper = synthetize_wrapper( struct_type );
                }
            }
            return wrappers;
        }


        struct replacer
        {

            void replace_pack()
            {
                for ( auto [original, fn ] : functions )
                    replace_pack( *fn );
            }

            template< typename inst_t, typename yield_t >
            bool walk( llvm::Value *val, yield_t yield )
            {
                if ( !val ) return false;
                auto inst = llvm::dyn_cast< inst_t >( val );
                if ( !inst ) return false;

                while ( inst )
                {
                    yield( inst );
                    inst = llvm::dyn_cast< inst_t >( inst->getAggregateOperand() );
                }
                return true;
            }

            values_t get( llvm::Function &f )
            {
                auto struct_t = get_original_type( &f );
                return values_t( struct_t->getNumElements(), nullptr );
            }

            llvm::StructType *get_original_type( llvm::Instruction *inst )
            {
                return get_original_type( inst->getParent()->getParent() );
            }

            llvm::StructType *get_original_type( llvm::Function *f )
            {
                auto ptr_t = llvm::dyn_cast< llvm::PointerType >( f->getReturnType() );
                return llvm::dyn_cast< llvm::StructType >( ptr_t->getElementType() );
            }

            void replace_returns( llvm::Instruction *ret, values_t args )
            {
                auto ret_t = get_original_type( ret );

                auto wrapper = wrappers.find( ret->getParent()->getParent() )->second;

                ASSERT( ret_t->getNumElements() == args.size());

                llvm::IRBuilder<> irb( ret );
                auto memory = irb.CreateCall( wrapper, args );
                irb.CreateRet( memory );

                ret->eraseFromParent();

            }

            void replace_pack( llvm::Function &f )
            {
                auto rets = query::query( f )
                            .flatten()
                            .filter( query::llvmdyncast< llvm::ReturnInst > )
                            .map( query::refToPtr )
                            .freeze();

                values_t args;
                values_t erasable;
                auto collect = [ & ]( llvm::InsertValueInst *insertvalue ) {
                    auto idx = *insertvalue->idx_begin();
                    auto val = insertvalue->getInsertedValueOperand();
                    args[ idx ] = val;
                    erasable.push_back( insertvalue );
                };

                for ( auto ret : rets )
                {
                    args = get( f );
                    walk< llvm::InsertValueInst >( ret->getOperand( 0 ), collect );

                    replace_returns( ret, std::move( args ) );

                    // Erase the insertvalues
                    erase( std::move( erasable ) );
                }
            }

            template< typename V >
            void erase( std::vector< V > erasable )
            {
                for ( auto v : erasable )
                {
                    auto inst = llvm::cast< llvm::Instruction >( v );
                    inst->replaceAllUsesWith( llvm::UndefValue::get( inst->getType() ) );
                    inst->eraseFromParent();
                }
            }

            void replace_calls()
            {
                std::vector< llvm::Instruction *> erasable;
                for ( auto [ original, func ] : functions )
                {
                    for ( auto user : original->users() )
                    {
                        if ( auto call = llvm::dyn_cast< llvm::CallInst >( user ) )
                        {
                            fix( call, func );
                            erasable.push_back( call );
                            continue;
                        }
                        std::cerr << "\nUnsupported fix" << std::endl;
                        user->print(llvm::errs());
                    }
                    erase( std::move( erasable ) );
                }
            }

            llvm::CallInst *rewire( llvm::CallSite old_cs, llvm::Function *new_f )
            {
                llvm::IRBuilder<> irb( old_cs.getInstruction() );
                return irb.CreateCall(
                        new_f,
                        values_t( old_cs.arg_begin(), old_cs.arg_end() ) );
            }

            void fix( llvm::CallInst *old_call, llvm::Function *new_f )
            {
                auto n_call = rewire( { old_call }, new_f );

                std::unordered_map< llvm::Instruction *, uint64_t > extracts;
                values_t erasable;
                auto collect = [ & ]( llvm::ExtractValueInst *extractvalue ) {
                    auto idx = *extractvalue->idx_begin();
                    extracts[ extractvalue ] = idx;
                    erasable.push_back( extractvalue );
                };

                for ( auto user : old_call->users() )
                {
                    walk< llvm::ExtractValueInst >( user, collect );
                    for ( auto [ val, idx ] : extracts )
                    {
                        llvm::IRBuilder<> irb( val );
                        auto gep = irb.CreateGEP( n_call,
                                                  { pass.i32( 0 ), pass.i32( idx ) } );
                        auto load = irb.CreateLoad( gep );
                        val->replaceAllUsesWith( load );
                    }
                }
                erase( std::move( erasable ) );
            }

            void replace_funcs()
            {
                for ( auto[ original, func ] : functions )
                {
                    auto name = original->getName();
                    original->eraseFromParent();
                    func->setName( name );
                }
            }

            void replace()
            {
                replace_pack();
                replace_calls();
                replace_funcs();
            }

            replacer( functions_map_t &functions_,
                      const agg_to_wrapper_t &wrappers_,
                      lower_ret_agg &pass_ )
                : functions( functions_ ),
                  wrappers( wrappers_ ),
                  pass( pass_ )
            {}

            functions_map_t &functions;
            const agg_to_wrapper_t &wrappers;
            lower_ret_agg &pass;
        };

        lower_ret_agg( llvm::Module &m ) : _m( &m ),
                                           _dl( m.getDataLayout() ),
                                           context( m.getContext() ) {}

        void run()
        {
            auto twins = change_ret_type();
            auto wrappers = synthetize_wrappers();
            replacer( twins, wrappers, *this ).replace();
            brick::llvm::verifyModule( _m );
        }

    };

}

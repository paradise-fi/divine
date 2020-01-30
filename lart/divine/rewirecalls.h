/*
 * (c) 2019 Lukáš Korenčik <xkorenc1@fi.muni.cz>
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

#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <bricks/brick-llvm>

namespace lart::divine {

// TODO: With LLVM-8.0 we will need to add CallBr at multiple places
struct rewire_calls_t {
    using rewired_to_t =
        std::unordered_map< llvm::Function *, std::unordered_set< llvm::Function * > >;

    llvm::Module &_module;
    llvm::LLVMContext &_ctx;

    uint64_t _counter = 0;
    const std::string wrap_prefix = "call.switch.box";

    // Information needed to properly build whole if-else tree in new dispatcher
    struct switch_box {
        llvm::Function *where;
        std::unordered_set< llvm::Function * > targets;

        // Last block, i.e. default case of switch, contains only unreachable instruction
        llvm::BasicBlock *tail;
    };

    std::unordered_map< llvm::Function *, switch_box > _func2tail;
    std::unordered_set< std::string > _wrappers;

    std::unordered_map< llvm::Function *, llvm::Function * > _box_to_original_fn;

    rewire_calls_t( llvm::Module &module )
        : _module( module ), _ctx( module.getContext() ) {}

    // Get new terminal node
    llvm::BasicBlock *_create_new_node( llvm::Function *func )
    {
        auto node = llvm::BasicBlock::Create( _ctx, "", func );

        llvm::IRBuilder<>{ node }.CreateUnreachable();
        return node;
    }

    void init();

    llvm::Function *_wrap( llvm::Type *original );

    llvm::BasicBlock *_create_call_bb( llvm::Function *where, llvm::Function *target );

    llvm::BasicBlock *_create_case( llvm::Function *where, llvm::Function *target );

    // Be sure that pointers are from the same Module!
    void _enhance( llvm::Function *where, llvm::Function *target )
    {
        ASSERT( _func2tail.count( where ),
            "Trying to enhance function that is not indirection.wrapper" );
        _func2tail[ where ].targets.insert( target );
        _func2tail[ where ].tail = _create_case( where, target );
    }

    void enhance( llvm::Function *where, llvm::Function *target )
    {
        // target is typically from clonned module -> we need to get declaration
        // into our module
        auto target_func = llvm::dyn_cast< llvm::Function >(
        _module.getOrInsertFunction( target->getName(), target->getFunctionType() ) );

        _enhance( _module.getFunction( where->getName() ), target_func );
    }

    void enhance( std::pair< llvm::Function *, llvm::Function *> new_call )
    {
        enhance( new_call.first, new_call.second );
    }

    // Matching on names, since we typically work with multiple modules
    bool is_wrapper( const std::string &name ) { return _wrappers.count( name ); }

    bool is_wrapper( const llvm::Function *func )
    {
        return func && is_wrapper( func->getName().str() );
    }

    llvm::Function *where( llvm::Function *box )
    {
        auto original_it = _box_to_original_fn.find( box );
        return ( original_it == _box_to_original_fn.end() ) ? nullptr : original_it->second;
    }

    rewired_to_t info()
    {
        rewired_to_t out;
        for ( auto [ func, box ] : _func2tail )
        {
            auto where_f = where( func );
            ASSERT( where_f );

            std::unordered_set< llvm::Function * > targets;
            for ( auto trg : box.targets )
                targets.insert( trg );
            out.insert( { where_f, std::move( targets ) } );
        }
        return out;
    }

    std::string report()
    {
        std::stringstream out;
        for ( auto [ func, box ] : _func2tail )
        {
            auto where_f = where( func );
            ASSERT( where_f );
            out << where_f->getName().str() << std::endl;
            for ( auto trg : box.targets )
                out << "\t" << trg->getName().str() << std::endl;
        }
        return out.str();
    }
};

static inline std::ostream &operator<<( std::ostream &os,
                                        const rewire_calls_t &self )
{
    os << "List switch boxes: " << std::endl;
    for ( const auto &f : self._func2tail )
    {
        os << "\t" << f.first->getName().str() << std::endl;
        f.first->dump();
    }
    return os;
}

} // namespace lart::divine

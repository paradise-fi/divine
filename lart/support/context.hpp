/*
 * (c) 2017 Vladimír Štill <xstill@fi.muni.cz>
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

DIVINE_RELAX_WARNINGS
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
DIVINE_UNRELAX_WARNINGS
#include <divine/vm/divm.h>
#include <string>

namespace lart::context {

using IRBuilder = llvm::IRBuilder<>;

struct Context
{
    virtual ~Context() = default;

    // all insert* functions insert at the current insertion point of the
    // builder and set the builder to insert next instruction after the (group
    // of) insrtruction they insert

    virtual void insertAssume( IRBuilder &, llvm::Value * ) = 0;
    virtual void insertCancel( IRBuilder &irb ) {
        insertAssume( irb, irb.getFalse() );
    }

    virtual void insertAssert( IRBuilder &, llvm::Value *,
                               std::string msg = "assertion failure" ) = 0;

    // returns value of that should be set to restore mask to the original state
    virtual llvm::Value *insertSetMask( IRBuilder & ) = 0;

    // takes value form insertSetMask
    virtual void insertRestoreMask( IRBuilder &, llvm::Value *restore ) = 0;

    virtual llvm::Function *getFreeFn() {
        return _module->getFunction( "free" );
    }

    virtual llvm::Function *getResizeFn() {
        return _module->getFunction( "realloc" );
    }

    virtual void setModule( llvm::Module &m ) {
        _module = &m;
        _clear();
    }

    // interpreter/virtual machine reserved functions
    virtual bool isInterpreterHypercall( llvm::Function & ) { return false; }

    // OS specific syscall (i.e. __dios_*, not Linux/POSIX syscalls)
    virtual bool isOSReserved( llvm::Function & ) { return false;  }

    // OS reserved or Hypercall
    virtual bool isSystemReserved( llvm::Function &fn ) {
        return isInterpreterHypercall( fn ) || isOSReserved( fn );
    }

    bool verbose = false;

  protected:
    virtual void _clear() { }
    llvm::Module *_module = nullptr;
};

namespace detail {

template< typename InsertAction >
void insertBranching( IRBuilder &irb, llvm::Value *b, InsertAction insertAction )
{
    auto it = irb.GetInsertPoint();
    auto *prebb = it->getParent();
    auto *postbb = prebb->splitBasicBlock( it, "br.true" );
    auto *failbb = llvm::BasicBlock::Create( irb.getContext(), "br.false", prebb->getParent() );
    auto *term = prebb->getTerminator();
    auto *condbr = llvm::BranchInst::Create( postbb, failbb, b );
    llvm::ReplaceInstWithInst( term, condbr );
    irb.SetInsertPoint( failbb->begin() );
    insertAction( irb );
    irb.SetInsertPoint( postbb->begin() );
}

}

struct DiVM : Context
{
    void insertAssume( IRBuilder &irb, llvm::Value *b ) override
    {
        detail::insertBranching( irb, b, [this]( IRBuilder &irb ) { insertCancel( irb ); } );
    }

    void insertCancel( IRBuilder &irb ) override
    {
        _context( irb, _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Cancel | _VM_CF_Mask | _VM_CF_Interrupted,
                                                 _VM_CF_Cancel | _VM_CF_Interrupted );
    }

    llvm::Value *insertSetMask( IRBuilder &irb ) override
    {
        auto *oldp = _context( irb, _VM_CA_Get, _VM_CR_Flags,
                                    _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask );
        auto *old = irb.CreatePtrToInt( oldp, irb.getInt64Ty() );
        return irb.CreateAnd( old, irb.getInt64( _VM_CF_Mask ) );
    }

    void insertRestoreMask( IRBuilder &irb, llvm::Value *restore ) override {
        _context( irb, _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, restore );
    }

    llvm::Function *getFreeFn() override {
        return _module->getFunction( "__vm_obj_free" );
    }

    llvm::Function *getResizeFn() override {
        return _module->getFunction( "__vm_obj_resize" );
    }

    bool isInterpreterHypercall( llvm::Function &fn ) override {
        return fn.getName().startswith( "__vm_" );
    }

  protected:
    void _clear() override
    {
        Context::_clear();
        _fControl = nullptr;
    }

    template< typename... Args >
    llvm::Value *_context( IRBuilder &irb, Args ... args )
    {
        return irb.CreateCall( _hypercallControl(), { _get( irb, args )... } );
    }

    // TODO: contexpr if when ready
    template< typename T >
    auto _get( IRBuilder &irb, T val )
        -> std::enable_if_t< (std::is_integral< T >::value || std::is_enum< T >::value)
                              && sizeof( T ) == 8, llvm::Value * >
    {
        return irb.getInt64( val );
    }

    template< typename T >
    auto _get( IRBuilder &irb, T val )
        -> std::enable_if_t< (std::is_integral< T >::value || std::is_enum< T >::value)
                             && sizeof( T ) == 4, llvm::Value * >
    {
        return irb.getInt32( val );
    }

    llvm::Value *_get( IRBuilder &, llvm::Value *val )
    {
        return val;
    }

    llvm::Function *_hypercallControl()
    {
        if ( _fControl )
            return _fControl;
        return _fControl = _module->getFunction( "__vm_control" );
    }

    bool isOSReserved( llvm::Function &fn ) override {
        return fn.getName().startswith( "__dios_" );
    }

  private:
    llvm::Function *_fControl = nullptr;
};

struct DiOS : DiVM
{
    void insertAssert( IRBuilder &irb, llvm::Value *b, std::string msg ) override
    {
        detail::insertBranching( irb, b, [&]( IRBuilder &irb ) {
                irb.CreateCall( _diosFault(), {
                                    irb.getInt32( _VM_F_Assert ),
                                    irb.CreateGlobalStringPtr( msg )
                                } );
            } );
    }

  protected:
    void _clear() override
    {
        DiVM::_clear();
        _fFault = nullptr;
    }

    llvm::Function *_diosFault()
    {
        if ( _fFault )
            return _fFault;
        return _fFault = _module->getFunction( "__dios_fault" );
    }

  private:
    llvm::Function *_fFault = nullptr;
};

} // namespace lart

// -*- C++ -*- (c) 2018 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/cleanup.h>
#include <divine/vm/divm.h>

namespace lart::divine {

struct LeakCheck
{
    bool _exit = false, _return = false, _suspend = false;

    LeakCheck( std::string opt )
    {
        if ( opt.find( "exit" ) != std::string::npos )
            _exit = true;
        if ( opt.find( "state" ) != std::string::npos )
            _suspend = true;
        if ( opt.find( "return" ) != std::string::npos )
            _return = true;
        if ( !_exit && !_return && !_suspend )
            throw std::logic_error( "constructed a useless LeakCheck instance" );
    }

    void run( llvm::Module &m )
    {
        auto *exit = m.getFunction( "_Exit" );
        auto *suspend = m.getFunction( "__dios_suspend" );
        auto *trace = m.getFunction( "__vm_trace" );

        for ( auto *fn : { exit, suspend } ) {
            if ( !fn || fn->empty() )
                continue;
            if ( !_exit && fn == exit )
                continue;
            if ( !_suspend && fn == suspend )
                continue;
            llvm::IRBuilder<> irb( &*fn->begin(), fn->begin()->getFirstInsertionPt() );
            irb.CreateCall( trace, { irb.getInt32( _VM_T_LeakCheck ) } );
        }

        auto ehInfo = cleanup::EhInfo::cpp( m );

        if ( !_return )
            return;

        for ( auto &fn : m )
        {
            if ( fn.empty() || &fn == suspend || &fn == exit )
                continue;
            cleanup::makeExceptionsVisible( ehInfo, fn, []( const auto & ) { return true; } );
            cleanup::atExits( fn, [&]( llvm::Instruction *i )
                    {
                        llvm::IRBuilder<> irb( i );
                        irb.CreateCall( trace, { irb.getInt32( _VM_T_LeakCheck ) } );
                    } );
        }
    }
};

PassMeta leakCheck()
{
    return passMetaO< LeakCheck >( "leak-check", "Enable memory leak checking" );
}

} // namespace lart::divine

#include <divine/sim/cli.hpp>
#include <lart/support/util.h>

namespace divine::sim {

std::set< llvm::StoreInst* > get_first_stores_to_alloca( llvm::AllocaInst * );
std::set< llvm::DbgValueInst* > get_first_dvis_of_var( llvm::DbgValueInst * );
void replace_until_next_dvi( llvm::BasicBlock::iterator, llvm::DILocalVariable *,
                             llvm::Value *, llvm::Value * );

/* Find the origin of a local variable in the original (pre-LART) LLVM module.
 * Depending of the variable's origin, it returns
 *   1) an llvm::Argument,
 *   2) an llvm::AllocaInst, or
 *   3) an llvm::DbgValueInst which is then tampered with specially. */
llvm::Value* CLI::find_tamperee( const DN &dn )
{
    if ( !dn._var_loc )
        return nullptr;

    auto *m = _bc->_pure_module.get();
    if ( auto *origin = llvm::dyn_cast< llvm::User >( dn._var_loc ) )
    {
        auto *f = m->getFunction( dn._di_inst->getFunction()->getName().str() );
        for ( auto &bb : *f )
        {
            for ( auto &inst : bb )
            {
                if ( llvm::isa< llvm::DbgDeclareInst >( inst ) ||
                     llvm::isa< llvm::DbgValueInst > ( inst ) )
                {
                    // llvm::DbgVariableIntrinsic in LLVM 9
                    auto *di = llvm::cast< llvm::DbgInfoIntrinsic >( &inst );
                    auto *divar = di->getVariable();

                    if ( divar &&
                         divar->getName().equals( dn._di_var->getName() ) &&
                         divar->getLine() == dn._di_var->getLine() &&
                         divar->getFilename().equals( dn._di_var->getFilename() ) &&
                         divar->getDirectory().equals( dn._di_var->getDirectory() ) &&
                         divar->getName().equals( dn._di_var->getName() ) )
                        return llvm::isa< llvm::DbgValueInst >( inst ) ? &inst : di->getVariableLocation();
                }
            }
        }
        return nullptr;
    }
    else if ( auto *origin = llvm::dyn_cast< llvm::Argument >( dn._var_loc ) )
    {
        auto *f = m->getFunction( origin->getParent()->getName().str() );
        unsigned argno = origin->getArgNo();
        return f->arg_begin() + argno;
    }
    else
        UNREACHABLE( "tamperee is something weird" );
}

namespace {
/* Set dummy location metadata (needed for call instructions) */
template< typename IRBuilder >
void prepareDebugMetadata( IRBuilder & irb )
{
    auto *discope = irb.GetInsertBlock()->getParent()->getSubprogram();
    auto dl = llvm::DebugLoc::get( 0, 0, discope, nullptr );
    irb.SetCurrentDebugLocation( dl );
}

/* Get correct __<domain>_<method>_<type> constructor, such as __sym_val_i64 */
template< typename DN >
llvm::Function* getAbstractConstructor( llvm::Module *m, DN & dn,
                                        const std::string & domain, const std::string & method )
{
    int width = dn.size() * 8;
    if ( width != 32 && width != 64 && width != 8 && width != 16 )
        throw brq::error( "unsupported width: " + std::to_string( width ) );

    std::string && name = "__" + domain + "_" + method + "_i" + std::to_string( width );
    auto fn = m->getFunction( name );
    if ( !fn )
        throw brq::error( "function \"" + name + "\" not found." );
    return fn;
}
} /* anonymous namespace */

/* Insert call to __<domain>_val_<type>() */
template< typename IRBuilder >
llvm::Value* CLI::mkCallNondet( IRBuilder & irb, DN &dn, const std::string & domain_name,
                                const std::string & name )
{
    auto aVal = getAbstractConstructor( irb.GetInsertBlock()->getModule(), dn, domain_name, "val" );
    prepareDebugMetadata( irb );
    return irb.CreateCall( aVal, llvm::NoneType::None, name + ".abstract" );
}

/* Insert call to __<domain>_lift_<type>( original_value ) */
template< typename IRBuilder >
llvm::Value* CLI::mkCallLift( IRBuilder & irb, DN & dn, const std::string & domain_name,
                              llvm::Value *original_value, const std::string & name )
{
    auto aLift = getAbstractConstructor( irb.GetInsertBlock()->getModule(), dn, domain_name, "lift" );
    prepareDebugMetadata( irb );
    llvm::Value * args[1] = { original_value };
    return irb.CreateCall( aLift, args, name + ".lifted" );
}

/* Tamper with a function argument -- replace all uses with nondet or lifted value */
void CLI::tamper( const command::tamper &cmd, DN &dn, llvm::Argument *arg )
{
    auto argname = arg->getName().str();

    llvm::IRBuilder<> irb( arg->getParent()->getEntryBlock().getFirstNonPHI() );
    if ( cmd.lift )
    {
        auto aval = mkCallLift( irb, dn, cmd.domain, arg, argname );
        arg->replaceAllUsesWith( aval );
        llvm::cast< llvm::User >( aval )->replaceUsesOfWith( aval, arg );
    }
    else
    {
        auto aval = mkCallNondet( irb, dn, cmd.domain, argname );
        arg->replaceAllUsesWith( aval );
    }
}

/* Tamper with an alloca'd variable -- replace first stores into it with nondet or lifted value */
void CLI::tamper( const command::tamper &cmd, DN &dn, llvm::AllocaInst *origin_alloca )
{
    auto varname = origin_alloca->getName().str();

    llvm::IRBuilder<> irb( origin_alloca->getNextNonDebugInstruction() );
    llvm::Value *aval = nullptr;

    // create nondet value to initialise the variable with
    if ( !cmd.lift )
        aval = mkCallNondet( irb, dn, cmd.domain, varname );

    // Find first stores to the alloca and replace them with abstract value
    std::set< llvm::StoreInst* > first_stores = get_first_stores_to_alloca( origin_alloca );

    if ( !first_stores.empty() ){

        // create freshness flag for the variable (1 = no store yet, 0 = a store has happened)
        auto &entry = origin_alloca->getFunction()->getEntryBlock();
        irb.SetInsertPoint( entry.getFirstNonPHIOrDbgOrLifetime() );
        auto *a_freshness = irb.CreateAlloca( irb.getInt1Ty(), nullptr, varname + ".fresh" );
        irb.SetInsertPoint( origin_alloca->getNextNonDebugInstruction() );
        irb.CreateStore( irb.getTrue(), a_freshness );

        for ( auto *store : first_stores )
        {
            auto *stored_orig = store->getValueOperand();

            if ( true /* TODO: heuristic */ )
            {
                if ( cmd.lift )
                {
                    // split the basic block and create branching: if the variable is fresh, lift the
                    // original value.
                    auto bb_upper = store->getParent();
                    std::string bb_name = bb_upper->getName();
                    auto bb_lower = bb_upper->splitBasicBlock( store, bb_name + ".cont" );
                    auto bb_lift = llvm::BasicBlock::Create( bb_lower->getContext(),
                            bb_name + ".lift." + varname, bb_lower->getParent(), bb_lower );
                    bb_upper->getTerminator()->eraseFromParent();

                    irb.SetInsertPoint( bb_upper );
                    auto *a_fresh = irb.CreateLoad( a_freshness, varname + ".isfresh" );
                    irb.CreateCondBr( a_fresh, bb_lift, bb_lower );

                    irb.SetInsertPoint( bb_lift );
                    auto *lifted = mkCallLift( irb, dn, cmd.domain, stored_orig, varname );
                    irb.CreateStore( irb.getFalse(), a_freshness );
                    irb.CreateBr( bb_lower );

                    irb.SetInsertPoint( &bb_lower->front() );
                    auto *phi = irb.CreatePHI( stored_orig->getType(), 2 );
                    phi->addIncoming( stored_orig, bb_upper );
                    phi->addIncoming( lifted, bb_lift );

                    store->replaceUsesOfWith( stored_orig, phi );
                }
                else
                {
                    // if fresh, store the nondet value; mark variable as non-fresh
                    irb.SetInsertPoint( store );
                    auto *a_fresh = irb.CreateLoad( a_freshness, varname + ".isfresh" );
                    auto *stored = irb.CreateSelect( a_fresh, aval, stored_orig,
                                                     varname + ".store" );
                    irb.CreateStore( irb.getFalse(), a_freshness );
                    store->replaceUsesOfWith( stored_orig, stored );
                }
            }
            else
            {
                // The straightforward case -- no dispatch needed
                irb.SetInsertPoint( store );
                if ( cmd.lift )
                    aval = mkCallLift( irb, dn, cmd.domain, store->getValueOperand(), varname );
                store->replaceUsesOfWith( store->getValueOperand(), aval );
            }
        }
    }
}

/* Tamper with a variable given by an llvm.dbg.value intrinsic -- replace all uses of the register
 * (or constant) until next llvm.dbg.value with nondet or lifted value */
void CLI::tamper( const command::tamper &cmd, DN &dn, llvm::DbgValueInst *dvi )
{
    auto varname = dvi->getVariableLocation()->getName().str();

    llvm::IRBuilder<> irb( dvi->getFunction()->getEntryBlock().getFirstNonPHI() );
    llvm::Value *aval = nullptr;

    // create nondet value to initialise the variable with
    if ( !cmd.lift )
        aval = mkCallNondet( irb, dn, cmd.domain, varname );

    std::set< llvm::DbgValueInst* > first_dvis = get_first_dvis_of_var( dvi );
    for ( auto *dvi : first_dvis )
    {
        if ( cmd.lift )
        {
            irb.SetInsertPoint( dvi );
            aval = mkCallLift( irb, dn, cmd.domain, dvi->getVariableLocation(), varname );
        }
        auto iit = dvi->getParent()->begin();
        while ( &*iit != dvi )
            ++iit;
        replace_until_next_dvi( ++iit, dvi->getVariable(), dvi->getVariableLocation(), aval );
        dvi->replaceUsesOfWith( dvi->getVariableLocation(), aval );
    }
}

template< typename Inst, typename Pred >
void bb_dfs_first_rec( llvm::BasicBlock *bb, Pred p, std::set< Inst* > &insts,
                       std::set< llvm::BasicBlock* > &visited )
{
    visited.insert( bb );
    for ( auto &inst : *bb )
    {
        auto *casted = llvm::dyn_cast< Inst >( &inst );
        if ( casted && p( casted ) )
        {
            insts.insert( casted );
            return;
        }
    }
    for ( auto &s : lart::util::succs( bb ) )
        if ( ! visited.count( &s ) )
            bb_dfs_first_rec( &s, p, insts, visited );
}

template< typename Inst, typename Pred >
std::set< Inst* > bb_dfs_first( llvm::BasicBlock *from, Pred p )
{
    std::set< llvm::BasicBlock* > visited;
    std::set< Inst* > insts;
    bb_dfs_first_rec( from, p, insts, visited );
    return insts;
}

// Return stores, that are potentially first to the given alloca
std::set< llvm::StoreInst* > get_first_stores_to_alloca( llvm::AllocaInst *origin_alloca )
{
    return bb_dfs_first< llvm::StoreInst >( origin_alloca->getParent(),
            [origin_alloca]( auto *store )
            {
                return store->getPointerOperand() == origin_alloca;
            });
}

// Return DVIs, that are potentially first definitions of the same variable as 'var_dvi'.
std::set< llvm::DbgValueInst* > get_first_dvis_of_var( llvm::DbgValueInst* var_dvi )
{
    auto *di_lvar = var_dvi->getVariable();
    return bb_dfs_first< llvm::DbgValueInst >( &( var_dvi->getFunction()->getEntryBlock() ),
            [di_lvar]( auto *dvi )
            {
                return dvi->getVariable() == di_lvar;
            });
}

void replace_until_next_dvi( llvm::BasicBlock::iterator iit, llvm::DILocalVariable *di_lvar,
                             llvm::Value *orig, llvm::Value *subst )
{
    auto *bb = iit->getParent();
    auto end = bb->end();
    while ( iit != end )
    {
        if ( auto *dvi = llvm::dyn_cast< llvm::DbgValueInst >( &*iit ) )
        {
            if ( dvi->getVariable() == di_lvar )
                return;
        }
        else if ( auto *u = llvm::dyn_cast< llvm::User >( &*iit ) )
            u->replaceUsesOfWith( orig, subst );
        ++iit;
    }

    for ( auto &s : lart::util::succs( bb ) )
        replace_until_next_dvi( s.begin(), di_lvar, orig, subst );
}

} /* divine::sim */

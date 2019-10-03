#include <lart/divine/indirectionstubber.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/CallSite.h>
DIVINE_UNRELAX_WARNINGS

namespace lart::divine {
// TODO: With LLVM-8.0 we will need to add CallBr at multiple places

// Copy argument list of called instruction and prepend it with called value
template< typename CallType >
std::vector< llvm::Value *> getArgumentList( CallType *inst ) {
  std::vector< llvm::Value *> new_args{ inst->getCalledValue() };
  new_args.insert( new_args.end(), inst->arg_begin(), inst->arg_end() );
  return new_args;
}

llvm::Value *createCall( llvm::CallInst *call, llvm::Function *callee ) {
  auto new_ret = llvm::IRBuilder<>{ call }.CreateCall( callee, getArgumentList( call ) );

  call->replaceAllUsesWith( new_ret );
  call->eraseFromParent();
  return new_ret;
}

llvm::Value *createCall( llvm::InvokeInst *invoke, llvm::Function *callee ) {
  auto new_ret = llvm::IRBuilder<>{ invoke }.CreateInvoke(
      callee,
      invoke->getNormalDest(),
      invoke->getUnwindDest(),
      getArgumentList( invoke ) );

  invoke->replaceAllUsesWith( new_ret );
  invoke->eraseFromParent();
  return new_ret;
}

template< typename T, typename = void >
struct is_iterable : std::false_type {};

template< typename T >
struct is_iterable< T, std::void_t< decltype( std::begin( std::declval< T >() ) ),
                                    decltype( std::end( std::declval< T >() ) ) > >
                  : std::true_type {};

template< typename T >
constexpr bool is_iterable_v = is_iterable< T >::value;

template< typename ... Next >
struct CallTypes {

  template< typename Func >
  void apply( Func ) {
    return;
  }

  template< typename From >
  void insert( From & ) {
    return;
  }
};


// Store chosen instructions into their own vector (casted to their type)
template < typename T, typename... Next >
struct CallTypes< T, Next... > {

  std::vector< T * > data;
  CallTypes< Next... > next;

  // Idea is that value in data is already casted to correct type
  template< typename Func >
  void apply( Func f )
  {
    for ( auto& d : data ) {
      f( d );
    }
    next.apply( f );
  }

  // Doest it cast to T? Add it to data, otherwise try next
  template< typename From >
  std::enable_if_t< !is_iterable_v< From > > insert( From &v )
  {
    if ( auto casted = llvm::dyn_cast< T >( &v ) ) {
      data.push_back( casted );
      return;
    }
    next.insert( v );
  }

  template< typename From >
  std::enable_if_t< is_iterable_v< From > > insert( From &from )
  {
    for ( auto &f : from ) {
      insert( f );
    }
  }


  template< typename From, typename UnaryPredicate >
  auto filter( From &from, UnaryPredicate pred ) ->
  std::enable_if_t< !is_iterable_v< From > >
  {
    if ( pred( from ) )
      insert( from );
  }

  template< typename From, typename UnaryPredicate >
  auto filter( From &from, UnaryPredicate pred ) ->
  std::enable_if_t< is_iterable_v< From > >
  {
    for ( auto &f : from ) {
      filter( f, pred );
    }
  }

  template< typename From >
  CallTypes< T, Next... >( From &from ) {
    insert( from );
  }

  template< typename From, typename UnaryPredicate >
  CallTypes< T, Next... >( From &from, UnaryPredicate pred ) {
    filter( from, pred );
  }


  CallTypes< T, Next... >() = default;

};

llvm::Function *IndirectCallsStubber::_wrap( llvm::Type *original ) {

  auto under =
    llvm::dyn_cast< llvm::FunctionType >( original->getPointerElementType() );

  std::vector< llvm::Type *> n_params = under->params();
  n_params.insert( n_params.begin(), original );

  auto n_f_type = llvm::FunctionType::get( under->getReturnType(), n_params, false );

  auto wrapper = llvm::Function::Create(
      n_f_type, llvm::GlobalValue::LinkageTypes::InternalLinkage,
      wrap_prefix + std::to_string(_counter++), &_module );

  wrapper->addFnAttr( llvm::Attribute::NoInline );

  _func2tail.insert( { wrapper, { wrapper, {}, _createNewNode( wrapper ) } } );
  _wrappers.insert( wrapper->getName().str() );

  return wrapper;
}

// Replaces all indirect calls or invoked with empty function that contains only
// unreachable instruction
void IndirectCallsStubber::stub() {

  auto is_indirect_call = [&]( auto &inst ) {
    auto cs = llvm::CallSite( &inst );
    return ( cs.isCall() || cs.isInvoke() ) && cs.isIndirectCall();
  };

  auto calls = CallTypes< llvm::CallInst, llvm::InvokeInst >( _module, is_indirect_call );

  auto wrap = [&]( auto &c ) {
    auto wrapper = this->_wrap( c->getCalledValue()->getType() );
    createCall( c, wrapper );
  };

  calls.apply( wrap );
}

llvm::BasicBlock *IndirectCallsStubber::_createCallBB(
    llvm::Function *where, llvm::Function *target ) {

  auto bb = llvm::BasicBlock::Create( _ctx, "C." + target->getName(),  where );
  llvm::IRBuilder<> ir{ bb };

  std::vector< llvm::Value *> forward_args;

  // Cast pointers if it's needed
  auto where_it = std::next( where->arg_begin() );
  for ( auto &target_arg : target->args() ) {
    llvm::Value *casted =
      ( where_it->getType()->isPointerTy() && target_arg.getType()->isPointerTy() ) ?
      ir.CreateBitCast( &*where_it, target_arg.getType() ) :
      &*where_it;

    forward_args.push_back( casted );
    ++where_it;
  }

  auto ret = ir.CreateCall( target, forward_args );

  if ( target->getReturnType()->isVoidTy() )
    ir.CreateRetVoid();
  else {
    ir.CreateRet( ret );
  }

  return bb;
}

llvm::BasicBlock *IndirectCallsStubber::_createCase(
    llvm::Function *where, llvm::Function *target ) {

  auto next = _createNewNode( where );
  auto succes = _createCallBB( where, target );

  auto current = _func2tail.at( where ).tail;
  ASSERT( llvm::isa< llvm::UnreachableInst > ( current->back() ) );

  llvm::IRBuilder<> ir( &current->back() );
  auto func_ptr = ir.CreateBitCast( &*where->arg_begin(), target->getType() );

  auto cmp = ir.CreateICmpEQ(
      func_ptr, target );
  ir.CreateCondBr( cmp, succes, next );

  // Remove old unreachable, otherwise bb will end up with 2 terminators
  current->back().eraseFromParent();

  return next;
}

} // namespace lart::divine

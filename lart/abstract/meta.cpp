#include <lart/abstract/meta.h>

#include <brick-assert>
#include <brick-data>
#include <brick-llvm>

namespace lart::abstract::meta {

    namespace {
        auto annotation( llvm::CallInst * call ) -> brick::llvm::Annotation {
            auto anno = brick::llvm::transformer( call ).operand( 1 )
                       .apply( [] ( auto val ) { return val->stripPointerCasts(); } )
                       .cast< llvm::GlobalVariable >()
                       .apply( [] ( auto val ) { return val->getInitializer(); } )
                       .cast< llvm::ConstantDataArray >()
                       .freeze();

            ASSERT( anno && "Call does not have annotation." );
            return brick::llvm::Annotation{ anno->getAsCString() };
        }

        auto functions_with_prefix( llvm::StringRef pref, llvm::Module & m ) noexcept {
            auto check = [pref] ( auto fn ) { return fn->getName().startswith( pref ); };
            return query::query( m ).map( query::refToPtr ).filter( check );
        }



        void set_value_as_meta( llvm::Instruction * inst, const std::string & tag, llvm::Value * val ) {
            auto &ctx = inst->getContext();
            auto meta = meta::tuple::create( ctx, { llvm::ValueAsMetadata::get( val ) } );
            inst->setMetadata( tag, meta );
        }

        llvm::Value * get_value_from_meta( llvm::Instruction * inst, const std::string & tag ) {
            auto &meta = inst->getMetadata( tag )->getOperand( 0 );
            return llvm::cast< llvm::ValueAsMetadata >( meta.get() )->getValue();
        }

        template< typename T >
        bool has_impl( T * value, const std::string & tag ) noexcept {
            return meta::get( value, tag).has_value();
        }

    } // anonymous namespace

    void create_from_annotation( llvm::StringRef anno, llvm::Module & m ) noexcept {
        for ( const auto &fn : functions_with_prefix( anno, m ) ) {
            for ( const auto &u : fn->users() ) {
                if ( auto call = llvm::dyn_cast< llvm::CallInst >( u ) ) {
                    auto inst = call->getOperand( 0 )->stripPointerCasts();
                    meta::abstract::set( inst, annotation( call ).name() );
                }
            }
        }
    }

    llvm::MDNode * create( llvm::LLVMContext & ctx, const std::string & str ) noexcept {
        return llvm::MDNode::get( ctx, llvm::MDString::get( ctx, str ) );
    }

    MetaVal value( llvm::MDNode * node, unsigned idx ) noexcept {
        if ( !node )
            return std::nullopt;
        auto & op = node->getOperand( idx );
        auto & str = llvm::cast< llvm::MDNode >( op )->getOperand( 0 );
        auto res = llvm::cast< llvm::MDString >( str )->getString().str();
        if (res.empty())
            return std::nullopt;
        return res;
    }

    MetaVal get( llvm::Value * val, const std::string & tag ) noexcept {
        if ( llvm::isa< llvm::Constant >( val ) )
            return std::nullopt;
        else if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
            return meta::get( arg, tag );
        else if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
            return meta::get( inst, tag );
        else
            UNREACHABLE( "Unsupported value" );
    }

    // TODO enable multiple metadata in argument
    MetaVal get( llvm::Argument * arg, const std::string & /* tag */ ) noexcept {
        return meta::argument::get( arg );
    }

    MetaVal get( llvm::Instruction * inst, const std::string & tag ) noexcept {
        return meta::value( inst->getMetadata( tag ) );
    }

    bool has( llvm::Value *val, const std::string & tag ) noexcept {
        return has_impl( val, tag );
    }

    bool has( llvm::Argument *arg, const std::string & tag ) noexcept {
        return has_impl( arg, tag );
    }

    bool has( llvm::Instruction *inst, const std::string & tag ) noexcept {
        return has_impl( inst, tag );
    }

    void set( llvm::Value * val, const std::string & tag, const std::string & meta ) noexcept {
        if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
            meta::set( arg, tag, meta );
        else if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
            meta::set( inst, tag, meta );
        else
            UNREACHABLE( "Unsupported value" );
    }

    void set( llvm::Argument * arg, const std::string & /*tag*/, const std::string & meta ) noexcept {
        // TODO enable multiple tags
        meta::argument::set( arg, meta );
    }

    void set( llvm::Instruction * inst, const std::string & tag, const std::string & meta ) noexcept {
        auto &ctx = inst->getContext();
        auto data = meta::tuple::create( ctx, { meta::create( ctx, meta ) } );
        inst->setMetadata( tag, data );
    }

    namespace abstract
    {
        bool has( llvm::Function * fn ) noexcept {
            return fn->getMetadata( meta::tag::roots );
        }

        void inherit( llvm::Value * dest, llvm::Value * src ) noexcept {
            if ( auto data = meta::abstract::get( src ) )
                meta::abstract::set( dest, data.value() );
        }
    } // namespace abstract

    namespace function
    {
        void init( llvm::Function * fn ) noexcept {
            auto size = fn->arg_size();

            auto & ctx = fn->getContext();

            if ( !fn->getMetadata( meta::tag::function ) ) {
                auto value = [&] { return meta::create( ctx, "" ); };
                auto data = meta::tuple::create( ctx, size, value );
                fn->setMetadata( meta::tag::function, data );
            }
        }

        void clear( llvm::Function * fn ) noexcept {
            if ( fn->getMetadata( meta::tag::function ) )
                fn->setMetadata( meta::tag::function, nullptr );
        }

        bool ignore_call( llvm::Function * fn ) noexcept {
            return fn->getMetadata( tag::transform::ignore::arg );
        }

        bool ignore_return( llvm::Function * fn ) noexcept {
            return fn->getMetadata( tag::transform::ignore::ret );
        }

        bool is_forbidden( llvm::Function * fn ) noexcept {
            return fn->getMetadata( tag::transform::forbidden );
        }
    } // namespace function

    namespace argument
    {
        // TODO enable multiple tags
        void set( llvm::Argument * arg, const std::string & str ) noexcept {
            auto & ctx = arg->getContext();
            argument::set( arg, meta::create( ctx, str ) );
        }

        void set( llvm::Argument * arg, llvm::MDNode * node ) noexcept {
            auto fn = arg->getParent();
            function::init( fn );

            auto meta = fn->getMetadata( meta::tag::function );
            meta->replaceOperandWith( arg->getArgNo(), node );
        }

        MetaVal get( llvm::Argument * arg ) noexcept {
            auto fn = arg->getParent();
            if ( auto node = fn->getMetadata( meta::tag::function ) ) {
                return meta::value( node, arg->getArgNo() );
            }
            return std::nullopt;
        }

        bool has( llvm::Argument * arg ) noexcept {
            return argument::get( arg ).has_value();
        }
    } // namespace argument

    void make_duals( llvm::Instruction * a, llvm::Instruction * b ) {
        set_value_as_meta( a, meta::tag::dual, b );
        set_value_as_meta( b, meta::tag::dual, a );
    }

    llvm::Value * get_dual( llvm::Instruction *inst ) {
        return get_value_from_meta( inst, meta::tag::dual );
    }

} // namespace lart::abstract::meta

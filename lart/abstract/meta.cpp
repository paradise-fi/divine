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
        if ( !node->getNumOperands() )
            return "";

        auto & op = node->getOperand( idx );
        auto & str = llvm::cast< llvm::MDNode >( op )->getOperand( 0 );
        auto res = llvm::cast< llvm::MDString >( str )->getString().str();
        if (res.empty())
            return std::nullopt;
        return res;
    }

    MetaVal get( llvm::Value * val, const std::string & tag ) noexcept {
        if ( auto fn = llvm::dyn_cast< llvm::Function >( val ) )
            return meta::get( fn, tag );
        else if ( auto glob = llvm::dyn_cast< llvm::GlobalVariable >( val ) )
            return meta::get( glob, tag );
        else if ( llvm::isa< llvm::Constant >( val ) )
            return std::nullopt;
        else if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
            return meta::get( arg, tag );
        else if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
            return meta::get( inst, tag );
        else
            UNREACHABLE( "Unsupported value" );
    }

    MetaVal get( llvm::Argument * arg, const std::string & /*tag*/ ) noexcept {
        // TODO enable multiple tags
        return meta::argument::get( arg );
    }

    MetaVal get( llvm::Instruction * inst, const std::string & tag ) noexcept {
        return meta::value( inst->getMetadata( tag ) );
    }

    MetaVal get( llvm::Function * fn, const std::string & tag ) noexcept {
        return meta::value( fn->getMetadata( tag ) );
    }

    MetaVal get( llvm::GlobalVariable * glob, const std::string & tag ) noexcept {
        return meta::value( glob->getMetadata( tag ) );
    }

    llvm::Value * get_value_from_meta( llvm::Instruction * inst, const std::string & tag ) {
        auto &meta = inst->getMetadata( tag )->getOperand( 0 );
        return llvm::cast< llvm::ValueAsMetadata >( meta.get() )->getValue();
    }

    void set( llvm::Value * val, const std::string & tag ) noexcept {
        set( val, tag, "" );
    }

    void set( llvm::Value * val, const std::string & tag, const std::string & meta ) noexcept {
        if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
            meta::set( arg, tag, meta );
        else if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
            meta::set( inst, tag, meta );
        else if ( auto fn = llvm::dyn_cast< llvm::Function >( val ) )
            meta::set( fn, tag, meta );
        else
            UNREACHABLE( "Unsupported value" );
    }

    void set( llvm::Argument * arg, const std::string & /*tag*/, const std::string & meta ) noexcept {
        // TODO enable multiple tags
        meta::argument::set( arg, meta );
    }


    template< typename T >
    void set_impl( T * val, const std::string & tag, const std::string & meta ) noexcept {
        auto &ctx = val->getContext();
        auto data = meta::tuple::create( ctx, { meta::create( ctx, meta ) } );
        val->setMetadata( tag, data );
    }

    void set( llvm::Instruction * inst, const std::string & tag, const std::string & meta ) noexcept {
        set_impl( inst, tag, meta );
    }

    void set( llvm::Function * fn, const std::string & tag, const std::string & meta ) noexcept {
        set_impl( fn, tag, meta );
    }

    namespace abstract
    {
        bool roots( llvm::Function * fn ) {
            return meta::has( fn, meta::tag::roots );
        }

        void inherit( llvm::Value * dest, llvm::Value * src ) noexcept {
            if ( auto data = meta::abstract::get( src ) )
                meta::abstract::set( dest, data.value() );
        }
    } // namespace abstract

    namespace function
    {
        constexpr auto abstract = meta::tag::function::abstract;
        constexpr auto duals = meta::tag::function::duals;

        void init( llvm::Function * fn ) noexcept {
            auto size = fn->arg_size();
            auto & ctx = fn->getContext();

            if ( !fn->getMetadata( abstract ) ) {
                auto value = [&] { return meta::create( ctx, "" ); };
                auto data = meta::tuple::create( ctx, size, value );
                fn->setMetadata( abstract, data );
            }

            if ( !fn->getMetadata( duals ) ) {
                auto empty = [&] { return meta::tuple::empty( ctx ); };
                auto data = meta::tuple::create( ctx, size, empty );
                fn->setMetadata( duals, data );
            }
        }

        void clear( llvm::Function * fn ) noexcept {
            if ( meta::has( fn, abstract ) )
                fn->setMetadata( abstract, nullptr );
        }

        bool ignore_call( llvm::Function * fn ) noexcept {
            return meta::has( fn, tag::transform::ignore::arg );
        }

        bool ignore_return( llvm::Function * fn ) noexcept {
            return meta::has( fn, tag::transform::ignore::ret );
        }

        bool is_forbidden( llvm::Function * fn ) noexcept {
            return meta::has( fn, tag::transform::forbidden );
        }

        bool placeholder( llvm::Function * fn ) noexcept {
            return fn->getMetadata( tag::placeholder::function );
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

            auto meta = fn->getMetadata( meta::tag::function::abstract );
            meta->replaceOperandWith( arg->getArgNo(), node );
        }

        MetaVal get( llvm::Argument * arg ) noexcept {
            auto fn = arg->getParent();
            if ( auto node = fn->getMetadata( meta::tag::function::abstract ) ) {
                return meta::value( node, arg->getArgNo() );
            }
            return std::nullopt;
        }

        bool has( llvm::Argument * arg ) noexcept {
            return argument::get( arg ).has_value();
        }

    } // namespace argument

    void set_value_as_meta( llvm::Instruction * inst, const std::string & tag, llvm::Value * val ) {
        auto &ctx = inst->getContext();
        auto meta = meta::tuple::create( ctx, { llvm::ValueAsMetadata::get( val ) } );
        inst->setMetadata( tag, meta );
    }

    void set_value_as_meta( llvm::Argument * arg, llvm::Value * val ) {
        auto fn = arg->getParent();
        function::init( fn );

        ASSERT( fn == llvm::cast< llvm::Instruction >( val )->getFunction() );
        auto &ctx = fn->getContext();
        auto node = meta::tuple::create( ctx, { llvm::ValueAsMetadata::get( val ) } );
        auto meta = fn->getMetadata( function::duals );
        meta->replaceOperandWith( arg->getArgNo(), node );
    }

    void make_duals( llvm::Value * a, llvm::Instruction * b ) {
        if ( auto arg = llvm::dyn_cast< llvm::Argument >( a ) )
            return make_duals( arg, b );
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( a ) )
            return make_duals( inst, b );
        UNREACHABLE( "Unsupported dual pair" );
    }

    void make_duals( llvm::Argument * arg, llvm::Instruction * inst ) {
        ASSERT( arg->getParent() == inst->getFunction() );
        set_value_as_meta( arg, inst );
        set_value_as_meta( inst, meta::tag::dual, arg );
    }

    void make_duals( llvm::Instruction * a, llvm::Instruction * b ) {
        ASSERT( a->getFunction() == b->getFunction() );
        set_value_as_meta( a, meta::tag::dual, b );
        set_value_as_meta( b, meta::tag::dual, a );
    }

    bool has_dual( llvm::Value * val ) {
        if ( llvm::isa< llvm::Constant >( val ) )
            return false;
        if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
            return has_dual( arg );
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
            return has_dual( inst );
        UNREACHABLE( "Unsupported dual value" );
    }

    bool has_dual( llvm::Argument * arg ) {
        return get_dual( arg );
    }

    bool has_dual( llvm::Instruction * inst ) {
        return inst->getMetadata( meta::tag::dual );
    }

    llvm::Value * get_dual( llvm::Value * val ) {
        if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
            return get_dual( arg );
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
            return get_dual( inst );
        UNREACHABLE( "Unsupported dual value" );
    }

    llvm::Value * get_dual( llvm::Argument * arg ) {
        auto fn = arg->getParent();
        if ( auto node = fn->getMetadata( function::duals ) ) {
            auto idx = arg->getArgNo();
            auto meta = llvm::cast< llvm::MDNode >( node->getOperand( idx ) );
            if ( meta->getNumOperands() == 0 )
                return nullptr;
            return llvm::cast< llvm::ValueAsMetadata >( meta->getOperand( 0 ).get() )->getValue();
        }
        return nullptr;
    }

    llvm::Value * get_dual( llvm::Instruction *inst ) {
        return get_value_from_meta( inst, meta::tag::dual );
    }

} // namespace lart::abstract::meta

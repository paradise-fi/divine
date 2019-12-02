// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/init.h>
#include <lart/support/query.h>
#include <lart/support/annotate.h>

#include <iostream>

using namespace std::literals;

namespace std
{
    template<> struct hash< lart::abstract::InitAbstractions::Operation >
    {
        using argument_type = lart::abstract::InitAbstractions::Operation;
        using result_type = std::size_t;
        result_type operator()( argument_type const& op ) const noexcept
        {
            using DomainT = lart::abstract::InitAbstractions::Domain;
            result_type const h1{ std::hash< llvm::Function * >{}( op.impl ) };
            result_type const h2{ std::hash< DomainT * >{}( op.domain ) };
            return h1 ^ (h2 << 1);
        }
    };
}

namespace lart::abstract {

    using Namespace = InitAbstractions::Namespace;
    using DomainT = InitAbstractions::Domain;
    using Domains = InitAbstractions::Domains;
    using Operation = InitAbstractions::Operation;

    constexpr const char * abstract_namespace = "_ZN6__dios3rst8abstract";
    constexpr const char * domain_constructor = "abstract_domain_t";

    auto all_operations( const Domains& doms )
    {
        auto get_ops = [] ( const auto & dom ) { return dom->operations; };
        return query::query( doms ).map( get_ops ).flatten().freeze();
    }

    template< typename I >
    llvm::ConstantInt * to_llvm_constant( llvm::LLVMContext &ctx, I value )
    {
        auto bw = std::numeric_limits< I >::digits;
        auto ty = llvm::Type::getIntNTy( ctx, bw );
        return llvm::ConstantInt::get( ty, value );
    }

    template< typename V >
    llvm::Constant * as_constant( const V &values )
    {
        ASSERT( !values.empty() );
        auto ty = llvm::ArrayType::get( values.front()->getType(), values.size() );
        return llvm::ConstantArray::get( ty, values );
    }

    template< typename I >
    void set_index_metadata( llvm::Function * fn, const char * meta, I index )
    {
        auto &ctx = fn->getContext();
        auto i = llvm::ConstantAsMetadata::get( to_llvm_constant( ctx, index ) );
        auto node = llvm::MDNode::get( ctx, { i } );
        fn->setMetadata( meta, node );
    }

    auto functions( llvm::Module &m, llvm::StringRef prefix, llvm::StringRef infix = "" )
    {
        auto has_prefix = [&] ( auto f ) { return f->getName().startswith( prefix ); };
        auto has_infix = [&] ( auto f ) { return f->getName().contains( infix ); };
        return query::query( m ).map( query::refToPtr ).filter( has_prefix ).filter( has_infix ).freeze();
    }

    std::vector< Namespace > abstract_namespaces( llvm::Module * m )
    {
        auto bases = functions( *m, abstract_namespace, domain_constructor );

        if ( bases.empty() )
            brq::raise() << "Missing abstract domain base constructor.";

        std::set< Namespace > lifts;
        for ( auto base : bases ) {
            auto l = query::query( base->users() )
                .map( query::llvmdyncast< llvm::CallInst > )
                .filter( query::notnull )
                .map( [] ( auto call ) {
                    return call->getFunction()->getName();
                } )
                .filter( [] ( auto name ) {
                    return name.contains( "lift_any" );
                } )
                .map( [] ( auto name ) {
                    return name.take_front( name.find( "lift_any" ) - 1 );
                } )
                .freeze();

            lifts.insert( l.begin(), l.end() );
        }

        return { lifts.begin(), lifts.end() };
    }

    // Sets correct indices of domain base constructor.
    struct ReindexDomainBasePass
    {
        ReindexDomainBasePass( const Domains & doms )
            : doms( doms )
        {}

        void run( llvm::Module & m )
        {
            // enumerate base calls
            auto bases = functions( m, abstract_namespace, domain_constructor );
            for ( auto base : bases ) {
                auto base_calls = query::query( base->users() )
                    .map( query::llvmdyncast< llvm::CallInst > )
                    .filter( query::notnull )
                    .freeze();

                // change base call indices
                for ( auto call : base_calls ) {
                    auto fn = call->getFunction();
                    call->setArgOperand( 1, get_domain_index( fn ) );
                }
            }
        }

        llvm::ConstantInt * get_domain_index( llvm::Function * fn )
        {
            auto is_op = [&] ( const auto &op ) { return op.impl == fn; };

            auto ops = all_operations( doms );
            if ( auto op = query::query( ops ).find( is_op ); op.found )
                return op.get().domain->llvm_index();
            else
                brq::raise() << "Using base domain class outside of abstract domain.";
            return nullptr;
        }

    private:
        const Domains & doms;
    };

    template< typename D >
    struct DomainsVTable
    {
        using Operations = std::vector< Operation >;
        using Domains = std::vector< D >;

        static constexpr const char * name = "__lart_domains_vtable";

        DomainsVTable( const Domains &doms )
            : doms( doms )
        {}

        void construct( llvm::Module &m ) const
        {
            auto &ctx = m.getContext();
            auto ops = operations(); // operation indices

            auto void_t = llvm::Type::getVoidTy( m.getContext() );

            for ( size_t i = 0; i < ops.size(); ++i ) {
                auto &op = ops[ i ];
                std::string name = "lart.abstract."s + op.name().str();
                auto fty = llvm::FunctionType::get( void_t, {}, false );
                auto fn = llvm::cast< llvm::Function >(
                    m.getOrInsertFunction( name, fty )
                );

                set_index_metadata( fn, meta::tag::operation::index, i );
            }

            std::vector< llvm::Constant * > domains;
            for ( const auto & dom : doms ) {
                auto row = row_data( ctx, dom, ops );
                domains.push_back( as_constant( row ) );
            }

            auto table = as_constant( domains );
            auto g = m.getOrInsertGlobal( name, table->getType() );
            auto global = llvm::cast< llvm::GlobalVariable >( g );

            global->setConstant( true );
            global->setLinkage( llvm::GlobalValue::InternalLinkage );
            global->setAlignment( 4 );
            global->setInitializer( table );
        }

        // Return a set of all operations in all domains
        //
        // Operations of domains index columns of VTable
        Operations operations() const
        {
            auto is_abstractable = [] ( const auto & op ) { return op.is(); };

            auto gt = [] ( const auto &a, const auto &b ) {
                return a.name() > b.name();
            };

            auto eq = [] ( const auto &a, const auto &b ) {
                return a.name() == b.name();
            };

            auto ops = query::query( all_operations( doms ) ).filter( is_abstractable ).freeze();

            std::sort( ops.begin(), ops.end(), gt );
            ops.erase( std::unique( ops.begin(), ops.end(), eq ), ops.end() );
            return ops;
        }

        auto row_data( llvm::LLVMContext &ctx, const D &dom, const Operations &ops ) const
        {
            std::vector< llvm::Constant * > functions;

            for ( const auto & op : ops ) {
                auto val = [&] () -> llvm::Constant * {
                    if ( auto dop = dom->get_operation( op.name() ) ) {
                        return dop->impl;
                    } else {
                        return llvm::ConstantPointerNull::get( op.impl->getType() );
                    }
                } ();
                // let pointers to function to have uniform type
                auto i8ptr = llvm::Type::getInt8PtrTy( ctx );
                auto ptr = llvm::ConstantExpr::getBitCast( val, i8ptr );
                functions.push_back( ptr );
            }

            return functions;
        }

        const Domains &doms;
    };

    llvm::StringRef Operation::name() const
    {
        auto n = impl->getName().substr( domain->ns.size() );
        auto num = n.take_while( [] ( auto c ) { return std::isdigit( c ); } );
        auto len = num.empty() ? std::string::npos : std::stoi( num.str() );
        return n.substr( num.size(), len );
    }

    std::optional< Operation > DomainT::get_operation( llvm::StringRef name ) const
    {
        auto is_op = [&] ( const auto & op ) { return op.name() == name; };
        if ( auto res = query::query( operations ).find( is_op ); res.found )
            return *res.iter;
        return std::nullopt;
    }

    void DomainT::annotate( index_t index ) const
    {
        auto lift = get_operation( "lift_any" ).value();
        set_index_metadata( lift.impl, domain_index, index );
    }

    Operation DomainT::lift() const
    {
        auto op = get_operation( "lift_any" );
        if ( !op.has_value() )
            brq::raise() << "Domain requires lift_any operation";
        return op.value();
    }

    llvm::ConstantInt * DomainT::llvm_index() const
    {
        auto i = lift().impl->getMetadata( domain_index );
        if ( !i )
            brq::raise() << "Missing domain index metadata.";
        auto c = llvm::cast< llvm::ConstantAsMetadata >( i->getOperand( 0 ) );
        return llvm::cast< llvm::ConstantInt >( c->getValue() );
    }

    DomainT::index_t DomainT::index() const
    {
        auto max = std::numeric_limits< index_t >::max();
        auto idx = llvm_index()->getLimitedValue( max );
        if ( idx == max )
            brq::raise() << "Ill-formed domain index.";
        return idx;
    }

    void InitAbstractions::annotate( const Domains & domains ) const
    {
        using index_t = DomainT::index_t;

        if ( domains.size() >= std::numeric_limits< index_t >::max() )
            brq::raise() << "Too many domains found";

        for ( index_t i = 0; i < domains.size(); ++i )
            domains[ i ]->annotate( i );
    }

    Domains InitAbstractions::domains() const
    {
        Domains doms;

        for ( auto ns : abstract_namespaces( module ) ) {
            auto dom = std::make_unique< DomainT >( ns );

            dom->operations = query::query( functions( *module, ns ) )
                .map( [&] ( auto fn ) { return Operation{ fn, dom.get() }; } )
                .freeze();
            doms.emplace_back( std::move( dom ) );
        }

        return doms;
    }

    void InitAbstractions::generate_get_domain_operation_intrinsic() const
    {
        auto fty = llvm::FunctionType::get( i8PTy(), { i32Ty(), i32Ty() }, false );
        auto fn = llvm::cast< llvm::Function >(
            module->getOrInsertFunction( "__lart_get_domain_operation", fty )
        );

        fn->deleteBody();

        auto & ctx = fn->getContext();
        auto entry = llvm::BasicBlock::Create( ctx, "entry", fn );

        llvm::IRBuilder<> irb( entry );

        auto domain = fn->arg_begin();
        auto op_index = std::next( fn->arg_begin() );
        auto table = module->getNamedGlobal( "__lart_domains_vtable" );

        auto ptr = irb.CreateGEP( table, { i64( 0 ), domain, op_index } );
        auto op = irb.CreateLoad( ptr );
        irb.CreateRet( op );
    }

    void InitAbstractions::run( llvm::Module &m )
    {
        module = &m;
        // 1. find all domains in module
        auto doms = domains();

        // 2. assign each domain an index
        annotate( doms );

        // 3. reindex domain base constructor arguments
        auto reindex = ReindexDomainBasePass( doms );
        reindex.run( m );

        // 4. generate and allocate domains vtable
        DomainsVTable vtable{ doms };
        vtable.construct( m );

        // TODO make all vtable functions invisible

        // TODO make all constructors invisible

        // 5. generate and allocate alfa-gamma table
        // TODO

        generate_get_domain_operation_intrinsic();
    }

} // namespace lart::abstract

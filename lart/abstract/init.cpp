// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/init.h>
#include <lart/support/query.h>

#include <iostream>

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

    // constexpr const char * abstraction_namespace = "_ZN6__dios3rst8abstract";

    constexpr const char * base_constructor = "_ZN6__dios3rst8abstract4BaseC2Eh";

    auto all_operations( const Domains& doms )
    {
        auto get_ops = [] ( const auto & dom ) { return dom->operations; };

        return query::query( doms ).map( get_ops ).flatten().freeze();
    }

    template< typename T >
    llvm::ConstantInt * to_llvm_constant( llvm::LLVMContext &ctx, T value )
    {
        auto bw = std::numeric_limits< T >::digits;
        auto ty = llvm::Type::getIntNTy( ctx, bw );
        return llvm::ConstantInt::get( ty, value );
    }

    template< typename Ctx, typename V >
    llvm::Constant * as_constant( Ctx &ctx, const V &values )
    {
        return llvm::ConstantStruct::getAnon( ctx, values );
    }


    std::vector< Namespace > abstract_namespaces( llvm::Module * m )
    {
        if ( auto base = m->getFunction( base_constructor ) ) {
            return query::query( base->users() )
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
        } else {
            ERROR( "Missing abstract domain base constructor." )
        }
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
            auto base = m.getFunction( base_constructor );
            if ( !base )
                ERROR( "Missing abstract domain base constructor." );

            auto base_calls = query::query( base->users() )
                .map( query::llvmdyncast< llvm::CallInst > )
                .filter( query::notnull )
                .freeze();

            // change base call indices
            for ( auto call : base_calls ) {
                call->setArgOperand( 1, get_domain_index( call->getFunction() ) );
            }
        }

        llvm::ConstantInt * get_domain_index( llvm::Function * fn )
        {
            auto is_op = [&] ( const auto &op ) { return op.impl == fn; };

            auto ops = all_operations( doms );
            if ( auto op = query::query( ops ).find( is_op ); op.found ) {
                return op.get().domain->llvm_index();
            } else {
                ERROR( "Using base domain class outside of abstract domain." )
            }
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

            std::vector< llvm::Constant * > domains;
            for ( const auto & dom : doms ) {
                auto row = row_data( dom, ops );
                domains.push_back( as_constant( ctx, row ) );
            }

            auto table = as_constant( ctx, domains );

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
            auto is_abstractable = [] ( const auto & op ) {
                return op.is_op() || op.is_fn();
            };

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

        auto row_data( const D &dom, const Operations &ops ) const
        {
            std::vector< llvm::Constant * > functions;

            for ( const auto & op : ops ) {
                if ( auto dop = dom->get_operation( op.name() ) ) {
                    functions.push_back( dop->impl );
                } else {
                    auto stub = llvm::ConstantPointerNull::get( op.impl->getType() );
                    functions.push_back( stub );
                }
            }

            return functions;
        }

        const std::vector< D > &doms;
    };

    llvm::StringRef Operation::name() const
    {
        auto n = impl->getName().substr( domain->ns.size() );
        auto len = n.take_while( [] ( auto c ) { return std::isdigit( c ); } );
        return n.substr( len.size(), std::stoi( len.str() ) );
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
        auto & ctx = lift.impl->getContext();

        auto i = llvm::ConstantAsMetadata::get( to_llvm_constant( ctx, index ) );
        auto meta = llvm::MDNode::get( ctx, { i } );
        lift.impl->setMetadata( domain_index, meta );
    }

    Operation DomainT::lift() const
    {
        auto op = get_operation( "lift_any" );
        if ( !op.has_value() )
            ERROR( "Domain requires lift_any operation" )
        return op.value();
    }

    llvm::ConstantInt * DomainT::llvm_index() const
    {
        auto i = lift().impl->getMetadata( domain_index );
        if ( !i )
            ERROR( "Missing domain index metadata." )
        auto c = llvm::cast< llvm::ConstantAsMetadata >( i->getOperand( 0 ) );
        return llvm::cast< llvm::ConstantInt >( c->getValue() );
    }

    DomainT::index_t DomainT::index() const
    {
        auto max = std::numeric_limits< index_t >::max();
        auto idx = llvm_index()->getLimitedValue( max );
        if ( idx == max )
            ERROR( "Ill-formed domain index." )
        return idx;
    }

    void InitAbstractions::annotate( const Domains & domains ) const
    {
        using index_t = DomainT::index_t;

        if ( domains.size() >= std::numeric_limits< index_t >::max() )
            ERROR( "Too many domains found" );

        for ( index_t i = 0; i < domains.size(); ++i )
            domains[ i ]->annotate( i );
    }

    Domains InitAbstractions::domains() const
    {
        Domains doms;

        for ( auto ns : abstract_namespaces( _m ) ) {
            auto in_namespace = [&] ( auto f ) { return f->getName().startswith( ns ); };

            auto dom = std::make_unique< DomainT >( ns );

            dom->operations = query::query( *_m )
                .map( query::refToPtr )
                .filter( in_namespace )
                .map( [&] ( auto fn ) { return Operation{ fn, dom.get() }; } )
                .freeze();

            doms.emplace_back( std::move( dom ) );
        }

        return doms;
    }

    void InitAbstractions::run( llvm::Module &m )
    {
        _m = &m;
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

        // 5. generate and allocate alfa-gamma table
        // TODO
    }

} // namespace lart::abstract

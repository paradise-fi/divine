// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>

#include <lart/abstract/annotation.h>

#include <lart/support/util.h>
#include <lart/abstract/util.h>
#include <lart/abstract/meta.h>

namespace lart::abstract {

    template< typename Value >
    void annotation_to_transform_metadata( llvm::StringRef ns, llvm::Module &m ) {
        auto &ctx = m.getContext();
        brick::llvm::enumerateAnnosInNs< Value >( ns, m, [&] ( auto val, auto anno ) {
            auto name = ns.str() + "." + anno.toString();
            val->setMetadata( name, meta::tuple::empty( ctx ) );
        });
    }

    template< typename Value >
    void annotation_to_domain_metadata( llvm::StringRef ns, llvm::Module &m ) {
        auto &ctx = m.getContext();
        brick::llvm::enumerateAnnosInNs< Value >( ns, m, [&] ( auto val, auto anno ) {
            auto meta = meta::create( ctx, anno.name() );
            val->setMetadata( ns, meta::tuple::create( ctx, { meta } ) );
        });
    }

    void LowerAnnotations::run( llvm::Module &m )
    {
        annotation_to_domain_metadata< llvm::Function >( meta::tag::abstract, m );
        annotation_to_transform_metadata< llvm::Function >( meta::tag::transform::prefix, m );

        for ( auto & fn : m ) {
            if ( auto meta = meta::get( &fn, meta::tag::abstract ) ) {
                for ( auto user : fn.users() ) {
                    if ( auto val = lower_constant_expr_call( user ) )
                        if ( auto call = llvm::dyn_cast< llvm::CallInst >( val ) )
                            meta::set( call, meta::tag::abstract, meta.value() );
                }
            }
        }
    }

} // namespace lart::abstract

// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
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

#include <lart/support/annotate.h>
#include <lart/support/util.h>

namespace lart {

    void LowerAnnotations::run( llvm::Module &m ) const noexcept
    {
        brick::llvm::enumerateFunctionsForAnno( _anno, m, [&] ( auto * fn ) { lower( fn ); } );
    }

    void LowerAnnotations::lower( llvm::Function * fn ) const noexcept
    {
        fn->addFnAttr( _anno );
    }

    void AnnotateFunctions::run( llvm::Module &m )
    {
        for ( auto & fn : m )
            if ( match( fn ) )
                annotate( fn );
    }

    bool AnnotateFunctions::match( const llvm::Function & fn ) const noexcept
    {
        auto name = fn.getName();
        return std::regex_match( name.begin(), name.end(), _rgx );
    }

    void AnnotateFunctions::annotate( llvm::Function & fn ) const noexcept
    {
        fn.addFnAttr( _anno );
    }

    using FunctionSet = std::set< llvm::Function * >;
    using FunctionMap = util::Map< llvm::Function *, llvm::Function * >;

    using AttrSet = std::set< llvm::StringRef >;
    using Attrs = std::vector< llvm::StringRef >;

    void PropagateRecursiveAnnotation::run( llvm::Module &m ) const noexcept
    {
        auto not_vm = []( llvm::Function &fn ) {
            return !fn.getName().startswith( "__vm_" );
        };

        auto const_null = []( auto & ) { return nullptr; };

        for ( auto [root, spread] : roots )
            LowerAnnotations( root ).run( m );

        auto set_attrs = [] ( auto fn, const auto & attrs ) {
            for ( auto attr : attrs )
                fn->addFnAttr( attr );
        };

        // collect attributes
        std::map< llvm::Function *, Attrs > attr_map;
        for ( const auto & [attr, spread] : roots )
            for ( auto fn : util::functions_with_attr( attr, m ) )
                attr_map[ fn ].push_back( spread );

        std::map< Attrs, FunctionMap > matched;

        auto matched_functions = [&] ( const auto & attrs ) {
            FunctionMap map;
            for ( auto [fn, clone] : matched[ attrs ] ) {
                map[ fn ] = clone;
                map[ clone ] = clone;
            }
            return map;
        };

        for ( const auto & [root, attrs] : attr_map ) {
            set_attrs( root, attrs );

            auto map = matched_functions( attrs );
            cloneCalleesRecursively( root, map, not_vm, const_null, true );

            for ( auto [fn, clone] : map ) {
                if ( ( fn != clone ) && !matched[ attrs ].count( fn ) ) {
                    set_attrs( clone, attrs );
                    matched[ attrs ][ fn ] = clone;
                }
            }
        }
    }

    PassMeta propagateRecursiveAnnotationPass() {
        return PropagateRecursiveAnnotation::meta();
    }

} // namespace lart

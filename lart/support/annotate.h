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

#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/support/meta.h>

#include <brick-llvm>
#include <regex>

namespace lart
{
    /* Lower function annotations to function attributes */
    struct LowerAnnotations
    {
        using Annotation = brick::llvm::Annotation;

        LowerAnnotations( std::string opt )
        {
            std::stringstream ss( opt );
            std::getline( ss, _anno );
        }

        static PassMeta meta() {
            return passMetaO< LowerAnnotations >( "lower-annot",
                    "options: annotation\n"
                    "\n"
                    "Lowers function annotations to function attributes.\n" );
        };


        void run( llvm::Module &m ) const noexcept;
        void lower( llvm::Function * fn ) const noexcept;

        std::string _anno;
    };

    /* Annotates all functions which name is matched by given regex */
    struct AnnotateFunctions
    {
        using Annotation = brick::llvm::Annotation;

        AnnotateFunctions( std::string opt )
        {
            std::stringstream ss( opt );

            std::getline( ss, _anno, ':' );

            std::string rgx;
            std::getline( ss, rgx, ':' );
            _rgx = std::regex{ rgx };
        }

        static PassMeta meta() {
            return passMetaO< AnnotateFunctions >( "annotate",
                    "options: annotation:regex\n"
                    "\n"
                    "Adds to matched functions attribute named 'annotation'.\n" );
        }

        void run( llvm::Module &m );
        bool match( const llvm::Function & fn ) const noexcept;
        void annotate( llvm::Function & fn ) const noexcept;

        std::string _anno;
        std::regex _rgx;
    };

    struct Interrupt
    {
        inline static const std::string skipmem = "lart.interrupt.skipmem";
        inline static const std::string local_skipmem = "lart.interrupt.local.skipmem";

        inline static const std::string skipcfl = "lart.interrupt.skipcfl";
        inline static const std::string local_skipcfl = "lart.interrupt.local.skipcfl";

        inline static void make_invisible( llvm::Function * fn ) noexcept
        {
            fn->addFnAttr( local_skipmem );
            fn->addFnAttr( local_skipcfl );
        }
    };

    struct PropagateRecursiveAnnotation
    {
        using Root = std::string;
        using Spread = std::string;

        using Roots = std::vector< std::pair< Root, Spread > >;

        PropagateRecursiveAnnotation()
            : roots( {
                { Interrupt::skipmem, Interrupt::local_skipmem },
                { Interrupt::skipcfl, Interrupt::local_skipcfl }
            } )
        {}

        static PassMeta meta() {
            return passMeta< PropagateRecursiveAnnotation >( "recurse-annotations", "" );
        }

        void run( llvm::Module &m ) const noexcept;

        Roots roots;
    };

    PassMeta propagateRecursiveAnnotationPass();

} // namespace lart

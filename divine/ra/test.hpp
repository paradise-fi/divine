/*
 * (c) 2020 Lukáš Korenčik  <xkorenc1@fi.muni.cz>
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

#include <iostream>
#include <memory>

#include <bricks/brick-assert>
#include <bricks/brick-llvm>

#include <divine/dbg/setup.hpp>
#include <divine/dbg/stepper.hpp>

#include <divine/mc/bitcode.hpp>
#include <divine/mc/job.hpp>

#include <divine/ra/base.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
DIVINE_UNRELAX_WARNINGS

#include <divine/ra/llvmrefine.hpp>

namespace divine::t_ra {

    struct base {
        static const bool dont_link = false;
        static const bool verbose = false;
        const std::vector< std::string > flags = { "-std=c++17" };

        std::shared_ptr< llvm::LLVMContext >_ctx = std::make_shared< llvm::LLVMContext >();

        auto compile( const std::string &src )
        {
            ::divine::rt::DiosCC c( { dont_link, verbose }, _ctx );
            c.setupFS( [ & ]( auto yield ){ yield( "/main.cpp", src ); } );
            c.link( c.compile( "/main.cpp", flags ) );
            return c.takeLinked();
        }

        auto minimal_bc( const std::string &src ) {
            divine::mc::BCOptions bc_opts;
            bc_opts.dios_config = "default";
            using bstr = divine::mc::BCOptions::bstr;
            bc_opts.bc_env = { { "divine.bcname",  bstr( src.begin(), src.end() ) } };
            return bc_opts;
        }
    };

    struct indirect_calls : base {
        using refinement_t = divine::ra::indirect_calls_refinement_t;
        using names_t = std::unordered_set< std::string >;
        using rewired_by_names_t =
            std::unordered_set< std::string, std::unordered_set< std::string > >;

        auto build( const std::string &src )
        {
            auto module = compile( src );
            auto bc_opts = minimal_bc( "main.bc" );
            return refinement_t( std::move( _ctx ), std::move( module ), bc_opts );
        }

        template< template< typename ...> class iterable_t >
        names_t to_names( iterable_t< llvm::Function * > iterable )
        {
          names_t out;
          for ( auto entry : iterable )
              // TODO: nullptr
              out.insert( entry->getName().str() );
          return out;
        }

        TEST( simple )
        {
            auto src = R"(
                extern "C" {
                    #define N __attribute__((__noinline__))
                    N void boo() { return; }
                    N void foo() { return; }

                    void dispatch( void(*f)() )
                    {
                        f();
                    }
                }
                int main()
                {
                    dispatch( boo );
                }
            )";

            auto refiner = build( src );
            refiner.finish();
            auto info = refiner._refiner.llvm_pass.info();
            ASSERT( info.size() == 1 );
            auto fn_name = info.begin()->first->getName().str();
            ASSERT( fn_name == "dispatch" );
            auto callees = to_names( info.begin()->second );
            ASSERT( callees == names_t{ "boo" } );
        }

        TEST( multiple_targets )
        {
            auto src = R"(
                #include <stdio.h>
                extern "C" {

                    #define N __attribute__((__noinline__))
                    N void boo() { puts( "A" ); }
                    N void foo() { puts( "B" ); }
                    N void goo() { puts( "C" ); }

                    N void dispatch( void(*f)() ) { f(); }
                }


                int main()
                {
                    dispatch( boo );
                    dispatch( goo );
                }
            )";
            auto refiner = build( src );
            refiner.finish();
            auto info = refiner._refiner.llvm_pass.info();
            ASSERT( info.size() == 1 );
            auto fn_name = info.begin()->first->getName().str();
            ASSERT( fn_name == "dispatch" );
            auto callees = to_names( info.begin()->second );
            ASSERT( callees == names_t{ "boo", "goo" } );
        }
    };

} // namespace t_ra


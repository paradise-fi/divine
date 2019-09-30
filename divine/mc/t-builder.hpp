// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016-2018 Petr Ročkai <code@fixp.eu>
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

#include <divine/mc/builder.hpp>
#include <divine/ss/search.hpp> /* unit tests */
#include <divine/mc/t-common.hpp>

namespace divine::t_mc
{
    using namespace std::literals;
    namespace
    {
        auto prog( std::string p )
        {
            return c2bc(
                "void *__vm_obj_make( int, int );"s +
                "void *__vm_ctl_get( int );"s +
                "void __vm_ctl_set( int, void * );"s +
                "long __vm_ctl_flag( long, long );"s +
                "void __vm_trace( int, const char * );"s +
                "int __vm_choose( int, ... );"s +
                "void __boot( void * );"s + p );
        }

        auto prog_int( std::string first, std::string next )
        {
            std::stringstream p;
            p << "void __sched() {" << std::endl
              << "    int *r = __vm_ctl_get( " << _VM_CR_State << " ); __vm_trace( 0, \"foo\" );" << std::endl
              << "    *r = " << next << ";" << std::endl
              << "    if ( *r < 0 ) __vm_ctl_flag( 0, 0b10000 );" << std::endl
              << "    __vm_ctl_set( " << _VM_CR_Frame << ", 0 );" << std::endl
              << "}" << std::endl
              << "void __boot( void *environ ) {"
              << "    __vm_ctl_set( " << _VM_CR_Scheduler << ", __sched );"
              << "    void *e = __vm_obj_make( sizeof( int ), " << _VM_PT_Heap << " );"
              << "    __vm_ctl_set( " << _VM_CR_State << ", e );"
              << "    int *r = e; *r = " << first << ";"
              << "    __vm_ctl_set( " << _VM_CR_Frame << ", 0 ); }" << std::endl;
            return prog( p.str() );
        }
    }

    struct TestBuilder
    {
        TEST(instance)
        {
            auto bc = prog( "void __boot( void *e ) { __vm_ctl_flag( 0, 0b10000 ); }" );
            mc::ExplicitBuilder ex( bc );
        }

        TEST(simple)
        {
            auto bc = prog_int( "4", "*r - 1" );
            mc::ExplicitBuilder ex( bc );
            bool found = false;
            ex.start();
            // ASSERT( setup::postboot_check( ex.context() ) );
            ex.initials( [&]( auto ) { found = true; } );
            ASSERT( found );
        }

        TEST(hasher)
        {
            auto bc = prog_int( "4", "*r - 1" );
            mc::ExplicitBuilder ex( bc );
            ex.start();
            ex.initials( [&]( auto s )
            {
                ASSERT( ex.hasher()._pool.size( s.snap ) );
            } );
        }

        TEST(hasher_copy)
        {
            auto bc = prog_int( "4", "*r - 1" );
            mc::ExplicitBuilder ex_( bc ), ex( ex_ );
            ex.start();
            ex.initials( [&]( auto s )
            {
                ASSERT( ex.hasher()._pool.size( s.snap ) );
                ASSERT( ex_.hasher()._pool.size( s.snap ) );
            } );
        }

        TEST(start_twice)
        {
            auto bc = prog_int( "4", "*r - 1" );
            mc::ExplicitBuilder ex( bc );
            mc::builder::State i1, i2;
            ex.start();
            ex.initials( [&]( auto s ) { i1 = s; } );
            ex.start();
            ex.initials( [&]( auto s ) { i2 = s; } );
            ASSERT( ex.equal( i1.snap, i2.snap ) );
        }

        TEST(copy)
        {
            auto bc = prog_int( "4", "*r - 1" );
            mc::ExplicitBuilder ex1( bc ), ex2( ex1 );
            mc::builder::State i1, i2;
            ex1.start();
            ex2.start();
            ex1.initials( [&]( auto i ) { i1 = i; } );
            ex2.initials( [&]( auto i ) { i2 = i; } );
            ASSERT( ex1.equal( i1.snap, i2.snap ) );
            ASSERT( ex2.equal( i1.snap, i2.snap ) );
        }

        TEST(succ)
        {
            auto bc = prog_int( "4", "*r - 1" );
            mc::ExplicitBuilder ex( bc );
            ex.start();
            mc::builder::State s1, s2;
            ex.initials( [&]( auto s ) { s1 = s; } );
            ex.edges( s1, [&]( auto s, auto, bool ) { s2 = s; } );
            ASSERT( !ex.equal( s1.snap, s2.snap ) );
        }

        void _search( std::shared_ptr< mc::BitCode > bc, int sc, int ec )
        {
            mc::ExplicitBuilder ex( bc );
            int edgecount = 0, statecount = 0;
            ex.start();
            ss::search( ss::Order::PseudoBFS, ex, 1, ss::passive_listen(
                            [&]( auto, auto, auto ) { ++edgecount; },
                            [&]( auto ) { ++statecount; } ) );
            ASSERT_EQ( statecount, sc );
            ASSERT_EQ( edgecount, ec );
        }

        TEST(search)
        {
            _search( prog_int( "4", "*r - 1" ), 5, 4 );
            _search( prog_int( "0", "( *r + 1 ) % 5" ), 5, 5 );
        }

        TEST(branching)
        {
            _search( prog_int( "4", "*r - __vm_choose( 2 )" ), 5, 9 );
            _search( prog_int( "4", "*r - 1 - __vm_choose( 2 )" ), 5, 7 );
            _search( prog_int( "0", "( *r + __vm_choose( 2 ) ) % 5" ), 5, 10 );
            _search( prog_int( "0", "( *r + 1 + __vm_choose( 2 ) ) % 5" ), 5, 10 );
        }
    };
}

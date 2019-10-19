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

#include <divine/mc/machine.hpp>
#include <divine/mc/ce.hpp>
#include <divine/mc/t-common.hpp>
#include <brick-cons>

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
                "void __vm_test_loop( int, void (*)() );"s +
                "void __vm_ctl_set( int, void * );"s +
                "long __vm_ctl_flag( long, long );"s +
                "int __vm_choose( int, ... );"s +
                "void __boot( void * );"s + p );
        }

        void sub_boot( std::stringstream &p )
        {
            p << "void __boot( void *environ )"
              << "{"
              << "    __vm_ctl_set( " << _VM_CR_Scheduler << ", __sched );"
              << "    void *e = __vm_obj_make( sizeof( int ), " << _VM_PT_Heap << " );"
              << "    __vm_ctl_set( " << _VM_CR_State << ", e );"
              << "    int *r = e; *r = init();"
              << "    __vm_ctl_set( " << _VM_CR_Frame << ", 0 );"
              << "}" << std::endl;
        }

        auto sub_sched( std::stringstream &p )
        {
            p << "void stop() { __vm_ctl_flag( 0, " << _VM_CF_Stop << " ); }";
            p << "void __sched()"
              << "{"
              << "    int *r = __vm_ctl_get( " << _VM_CR_State << " );"
              << "    __vm_ctl_flag( " << _VM_CF_IgnoreLoop << ", 0 );"
              << "    do"
              << "    {"
              << "        *r = next( *r );"
              << "        if ( *r < 0 ) __vm_ctl_flag( 0, " << _VM_CF_Cancel << " );"
              << "        if ( error( *r ) ) __vm_ctl_flag( 0, " << _VM_CF_Error << " );"
              << "        __vm_test_loop( 0, stop );"
              << "    } while ( loop( *r ) );"
              << "    __vm_ctl_set( " << _VM_CR_Frame << ", 0 );"
              << "}" << std::endl;
        }

        auto prog_int( int first, std::string next, std::string loop = "0", std::string error = "0" )
        {
            std::stringstream p;
            p << "int next( int x ) { return " << next << "; }" << std::endl
              << "int loop( int x ) { return " << loop << "; }" << std::endl
              << "int error( int x ) { return " << error << "; }" << std::endl
              << "int init() { return " << first << "; }" << std::endl;

            sub_sched( p );
            sub_boot( p );

            return prog( p.str() );
        }

        auto prog_err( int first, std::string next, std::string error = "0" )
        {
            return prog_int( first, next, "0", error );
        }

        auto prog_loop( int first, std::string next, std::string loop = "0" )
        {
            return prog_int( first, next, loop, "0" );
        }
    }

    struct Machine
    {
        TEST( instance )
        {
            mc::tmachine tm;
            mc::gmachine gm;
            mc::computer cm;
        }

        auto computer( mc::BC bc ) { mc::computer m; m.bc( bc ); return m; }

        TEST( simple )
        {
            bool found = false;
            auto match = [&]( mc::event::edge ) { found = true; };
            auto c = computer( prog_int( 4, "x - 1" ) );
            mc::tmachine tree;
            mc::weave( c, tree ).observe( match ).start();
            ASSERT( found );
        }

        TEST( hasher )
        {
            bool ran = false;
            auto c = computer( prog_int( 4, "x - 1" ) );
            mc::gmachine graph;
            auto check = [&]( mc::event::edge s )
            {
                TRACE( "check edge", s.from, s.to );
                ASSERT( graph.hasher()._pool.size( s.from ) );
                ran = true;
            };

            mc::weave( graph, c ).observe( check ).start();
            ASSERT( ran );
        }

#if 0
        TEST( start_twice )
        {
            auto machine = tmachine( prog_int( 4, "x - 1" ) );
            mc::State i1, i2;

            mc::weave( machine ).observe( [&]( Expand e ) { i1 = e.from; } ).start();
            mc::weave( machine ).observe( [&]( Expand e ) { i2 = e.from; } ).start();

            ASSERT( machine.equal( i1.snap, i2.snap ) );
        }

        TEST( copy )
        {
            mc::State i1, i2;

            auto m1 = tmachine( prog_int( 4, "x - 1" ) ), m2 = m1;
            mc::weave( m1 ).observe( [&]( Expand e ) { i1 = e.from; } ).start();
            mc::weave( m2 ).observe( [&]( Expand e ) { i2 = e.from; } ).start();

            ASSERT( m1.equal( i1.snap, i2.snap ) );
            ASSERT( m2.equal( i1.snap, i2.snap ) );
        }

        TEST( unfold )
        {
            std::vector< mc::Snapshot > snaps;
            auto machine = tmachine( prog_int( 4, "x - 1" ) );

            auto state = [&]( Expand e )
            {
                snaps.push_back( e.from.snap );
                if ( snaps.size() == 2 )
                    ASSERT( !machine.equal( snaps[ 0 ], snaps[ 1 ] ) );
            };

            mc::weave( machine ).extend( Search() ).observe( state ).start();
            ASSERT_LEQ( 2, snaps.size() );
        }
#endif

        template< typename... M >
        void _search( std::shared_ptr< mc::BitCode > bc, int sc, int ec )
        {
            brq::cons_list_t< mc::computer, M... > machines;
            int edgecount = 0, statecount = 0;
            auto edge = [&]( mc::event::edge e ) { ++edgecount; if ( e.is_new ) ++ statecount; };
            auto observe = [&]( auto & ... m )
            {
                mc::weave( m... ).observe( edge ).start();
            };

            machines.car().bc( bc );
            machines.apply( observe );

            ASSERT_EQ( statecount, sc );
            ASSERT_EQ( edgecount, ec );
        }

        using TM = mc::tmachine;
        using GM = mc::gmachine;
        using CM = mc::machine::graph_ce;

        TEST( cf_loop ) { _search< GM >( prog_int( 0, "x < 2 ? x + 1 : 2" , "x == 2" ), 1, 1 ); }

        TEST( search1 ) { _search< TM >( prog_int( 4, "x - 1" ), 5, 5 ); }
        TEST( search2 ) { _search< GM >( prog_int( 4, "x - 1" ), 4, 4 ); }
        TEST( search3 ) { _search< GM >( prog_int( 0, "( x + 1 ) % 5" ), 4, 5 ); }

        TEST( branching1 ){ _search< GM >( prog_int( 4, "x - __vm_choose( 2 )" ), 4, 9 ); }
        TEST( branching2 ){ _search< GM >( prog_int( 2, "x - 1 - __vm_choose( 2 )" ), 2, 3 ); }
        TEST( branching3 ){ _search< TM >( prog_int( 4, "x - 1 - __vm_choose( 2 )" ), 12, 12 ); }
        TEST( branching4 ){ _search< GM >( prog_int( 4, "x - 1 - __vm_choose( 2 )" ), 4, 7 ); }
        TEST( branching5 ){ _search< GM >( prog_int( 0, "( x + __vm_choose( 2 ) ) % 5" ), 4, 10 ); }
        TEST( branching6 ){ _search< GM >( prog_int( 0, "( x + 1 + __vm_choose( 2 ) ) % 5" ), 4, 10 ); }
    };

}

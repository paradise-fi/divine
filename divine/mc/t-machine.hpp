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

namespace divine::t_mc
{

    using namespace std::literals;

    namespace
    {
        auto prog( std::string p )
        {
            return t_vm::c2bc(
                "void *__vm_obj_make( int, int );"s +
                "void *__vm_ctl_get( int );"s +
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
            p << "void __sched()"
              << "{"
              << "    int *r = __vm_ctl_get( " << _VM_CR_State << " );"
              << "    do"
              << "    {"
              << "        *r = next( *r );"
              << "        if ( *r < 0 ) __vm_ctl_flag( 0, " << _VM_CF_Cancel << " );"
              << "        if ( loop( *r ) ) __vm_ctl_flag( 0, " << _VM_CF_Stop << ");"
              << "    } while ( loop( *r ) );"
              << "    __vm_ctl_set( " << _VM_CR_Frame << ", 0 );"
              << "}" << std::endl;
        }

        auto prog_int( int first, std::string next, std::string loop = "0" )
        {
            std::stringstream p;
            p << "int next( int x ) { return " << next << "; }" << std::endl
              << "int loop( int x ) { return " << loop << "; }" << std::endl
              << "int init() { return " << first << "; }" << std::endl;

            sub_sched( p );
            sub_boot( p );

            return prog( p.str() );
        }
    }

    struct Machine
    {
        TEST( instance )
        {
            auto bc = prog( "void __boot( void *e ) { __vm_ctl_flag( 0, 0b10000 ); }" );
            mc::TMachine m( bc );
        }

        using Search = mc::Search< mc::TQ >;
        using Expand = mc::TQ::Tasks::Expand;
        using Edge = mc::TQ::Tasks::Edge;

        template< typename M >
        auto weave( M &machine )
        {
            return mc::Task::Weaver().extend_ref( machine );
        }

        TEST( simple )
        {
            bool found = false;
            auto state = [&]( Expand ) { found = true; };
            auto machine = mc::TMachine( prog_int( 4, "x - 1" ) );
            weave( machine ).observe( state ).start();
            ASSERT( found );
        }

        TEST( hasher )
        {
            bool ran = false;
            auto machine = mc::GMachine( prog_int( 4, "x - 1" ) );
            auto check = [&]( Expand s )
            {
                ASSERT( machine.hasher()._pool->size( s.from.snap ) );
                ran = true;
            };

            weave( machine ).observe( check ).start();
            ASSERT( ran );
        }

        TEST( start_twice )
        {
            auto machine = mc::TMachine( prog_int( 4, "x - 1" ) );
            mc::State i1, i2;

            weave( machine ).observe( [&]( Expand e ) { i1 = e.from; } ).start();
            weave( machine ).observe( [&]( Expand e ) { i2 = e.from; } ).start();

            ASSERT( machine.equal( i1.snap, i2.snap ) );
        }

        TEST( copy )
        {
            mc::State i1, i2;

            auto m1 = mc::TMachine( prog_int( 4, "x - 1" ) ), m2 = m1;
            weave( m1 ).observe( [&]( Expand e ) { i1 = e.from; } ).start();
            weave( m2 ).observe( [&]( Expand e ) { i2 = e.from; } ).start();

            ASSERT( m1.equal( i1.snap, i2.snap ) );
            ASSERT( m2.equal( i1.snap, i2.snap ) );
        }

        TEST( unfold )
        {
            std::vector< mc::Snapshot > snaps;
            auto machine = mc::TMachine( prog_int( 4, "x - 1" ) );

            auto state = [&]( Expand e )
            {
                snaps.push_back( e.from.snap );
                if ( snaps.size() == 2 )
                    ASSERT( !machine.equal( snaps[ 0 ], snaps[ 1 ] ) );
            };

            weave( machine ).extend( Search() ).observe( state ).start();
            ASSERT_LEQ( 2, snaps.size() );
        }

        template< typename M >
        void _search( std::shared_ptr< mc::BitCode > bc, int sc, int ec )
        {
            M machine( bc );
            int edgecount = 0, statecount = 0;
            auto edge = [&]( Edge ) { ++edgecount; };
            auto state = [&]( Expand ) { ++statecount; };

            weave( machine ).extend( Search() ).observe( edge, state ).start();

            ASSERT_EQ( statecount, sc );
            ASSERT_EQ( edgecount, ec );
        }

        TEST( search )
        {
            _search< mc::TMachine >( prog_int( 4, "x - 1" ), 5, 4 );
            _search< mc::GMachine >( prog_int( 4, "x - 1" ), 5, 4 );
            _search< mc::GMachine >( prog_int( 0, "( x + 1 ) % 5" ), 5, 5 );
        }

        TEST( branching )
        {
            _search< mc::GMachine >( prog_int( 4, "x - __vm_choose( 2 )" ), 5, 9 );
            _search< mc::GMachine >( prog_int( 2, "x - 1 - __vm_choose( 2 )" ), 3, 3 );
            _search< mc::TMachine >( prog_int( 4, "x - 1 - __vm_choose( 2 )" ), 12, 11 );
            _search< mc::GMachine >( prog_int( 4, "x - 1 - __vm_choose( 2 )" ), 5, 7 );
            _search< mc::GMachine >( prog_int( 0, "( x + __vm_choose( 2 ) ) % 5" ), 5, 10 );
            _search< mc::GMachine >( prog_int( 0, "( x + 1 + __vm_choose( 2 ) ) % 5" ), 5, 10 );
        }
    };

}

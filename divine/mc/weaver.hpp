// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018-2019 Petr Ročkai <code@fixp.eu>
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
#include <deque>
#include <brick-cons>

namespace divine::mc::task
{
    struct base
    {
        uint32_t msg_id = 0;
        int16_t  msg_from = 0;
        int16_t  msg_to; /* -1 = any, -2 = all, >= 0 specific machine */

        base( int to = -1 ) : msg_to( to ) {}
        bool valid() { return msg_to >= -2; }
        void cancel() { msg_to = -3; }
    };

    struct start : base {};
}

namespace divine::mc
{
    template< typename T >
    using qref = std::deque< T > &;

    template< typename... tasks >
    using task_queue = typename brq::cons_list_t< tasks... >::unique_t::template map_t< qref >;

    struct machine_base
    {
        int16_t  _machine_id = -1;
        int16_t  _last_msg_from = -1;
        uint32_t _last_msg_id;

        int machine_id()
        {
            ASSERT_NEQ( _machine_id, -1 );
            return _machine_id;
        }

        void machine_id( int i )
        {
            // ASSERT_EQ( _machine_id, -1 );
            ASSERT_LEQ( 0, i );
            _machine_id = i;
        }

        void prepare( const task::base &t )
        {
            _last_msg_from = t.msg_from;
            _last_msg_id = t.msg_id;
        }

        template< typename task_t >
        void push( qref< std::remove_reference_t< task_t > > q, task_t &&t )
        {
            t.msg_from = machine_id();
            q.push_back( t );
        }

        template< typename task_t >
        void reply( qref< std::remove_reference_t< task_t > > q, task_t &&t )
        {
            ASSERT_LEQ( 0, _last_msg_from );
            t.msg_to = _last_msg_from;
            t.msg_id = _last_msg_id;
            push( q, t );
        }
    };

    template< typename tq, typename yield_t >
    bool pop( tq &q, yield_t yield )
    {
        if constexpr ( tq::empty )
            return false;
        else
        {
            if ( q.car().empty() )
                return pop( q.cdr(), yield );
            else
            {
                yield( q.car().front() );
                q.car().pop_front();
                return true;
            }
        }
    }

    template< typename tq, typename... tasks >
    using task_queue_extend = typename task_queue< tasks... >::template cat_t< tq >::unique_t;

    template< typename tq1, typename tq2 >
    using task_queue_join = typename tq1::template cat_t< tq2 >::unique_t;

    template< typename F >
    struct Observer : machine_base
    {
        F f;
        Observer( F f ) : f( f ) {}

        template< typename T >
        auto run( T t )
        {
            if constexpr ( std::is_invocable_v< F, T & > )
                return f( t );
        }
    };

    template< typename TQ, typename F >
    struct Lambda : machine_base
    {
        using tq = TQ;

        F f;
        Lambda( F f ) : f( f ) {}

        template< typename T >
        auto run( TQ tq, T &t ) -> decltype( f( *this, tq, t ) )
        {
            return f( *this, tq, t );
        }
    };

    template< typename M >
    struct Ref : std::reference_wrapper< M >
    {
        using std::reference_wrapper< M >::reference_wrapper;
        using tq = typename M::tq;

        void machine_id( int i ) { this->get().machine_id( i ); }
        int machine_id() { return this->get().machine_id(); }

        template< typename task_t >
        void prepare( const task_t &t )
        {
            this->get().prepare( t );
        }

        template< typename TQ, typename T >
        auto run( TQ tq, const T &t ) -> decltype( this->get().run( tq, t ) )
        {
            return this->get().run( tq, t );
        }
    };

    template< typename TQ, typename... Machines >
    struct Weaver
    {
        using MachineT = brq::cons_list_t_< Machines... >;
        MachineT _machines;
        typename TQ::template map_t< std::remove_reference_t > _queue;

        template< typename... ExMachines >
        using Extend = Weaver< TQ, Machines..., ExMachines... >;

        template< typename... ExMachines >
        auto extend()
        {
            return Extend< ExMachines... >();
        }

        template< typename... ExMachines >
        auto extend( ExMachines... exm )
        {
            return Extend< ExMachines... >( _machines.cat( brq::cons_list( exm... ) ) );
        }

        template< typename... R >
        auto extend_ref( R & ... ref )
        {
            return extend( Ref< R >( ref )... );
        }

        template< typename... F >
        auto observe( F... fs )
        {
            return extend( Observer< F >( fs ) ... );
        }

        template< typename... F >
        auto extend_f( F... fs )
        {
            return extend( Lambda< TQ, F >( fs ) ... );
        }

        void init_ids()
        {
            int i = 0;
            _machines.each( [&]( auto &m ) { m.machine_id( i++ ); } );
        }

        Weaver( MachineT mt ) : _machines( mt ) { init_ids(); }
        Weaver() { init_ids(); }

        template< typename T > T &machine()
        {
            return _machines.template get< T >();
        }

        template< typename M, typename T >
        auto run_on( M &m, T &t )
            -> decltype( m.run( _queue.template view< typename M::tq >(), t ), true )
        {
            m.prepare( t );
            m.run( _queue.template view< typename M::tq >(), t );
            return true;
        }

        template< typename M, typename T >
        auto run_on( M &m, T &t ) -> decltype( m.run( t ), true )
        {
            m.run( t );
            return true;
        }

        template< typename M >
        auto run_on( M &, brq::fallback )
            -> decltype( _queue.template view< typename M::tq >(), false )
        {
            return false;
        }

        template< typename T, typename... Args >
        void add( Args... args )
        {
            qref< T > q = _queue;
            q.emplace_back( args... );
        }

        void run() /* TODO parallel */
        {
            auto process = [&]( auto t )
            {
                int i = 0;

                auto one = [&]( auto &m )
                {
                    TRACE( "trying machine", i, "to =", t.msg_to, "valid =", t.valid() );
                    if ( t.valid() )
                        if ( t.msg_to == i || t.msg_to < 0 )
                            if ( run_on( m, t ) ) /* accepted */
                            {
                                TRACE( "task", t, "accepted by", &m );
                                if ( t.msg_to == -1 ) /* was targeted to anyone */
                                    t.msg_to = -3;
                            }
                    i ++;
                };

                _machines.each( one );
            };

            while ( pop( _queue, process ) );
        }

        void start()
        {
            add< task::start >();
            run();
        }
    };

}

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
    struct base { bool valid() { return true; } };
    struct start : base {};
}

namespace divine::mc
{
    template< typename T >
    using deque = std::deque< T > &;

    template< typename... tasks >
    using task_queue = typename brq::cons_list_t< tasks... >::unique_t::template map_t< deque >;

    template< typename task_t, typename... args_t >
    void push( deque< task_t > q, args_t && ... args )
    {
        q.emplace_back( std::forward< args_t >( args )... );
    }

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
    struct Observer
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
    struct Lambda
    {
        using tq = TQ;

        F f;
        Lambda( F f ) : f( f ) {}

        template< typename T >
        auto run( TQ tq, T &t ) -> decltype( f( tq, t ) )
        {
            return f( tq, t );
        }
    };

    template< typename M >
    struct Ref : std::reference_wrapper< M >
    {
        using std::reference_wrapper< M >::reference_wrapper;
        using tq = typename M::tq;

        template< typename TQ, typename T >
        auto run( TQ tq, const T &t ) -> decltype( this->get().run( tq, t ) )
        {
            return this->get().run( tq, t );
        }
    };

    template< typename TQ, typename... Machines >
    struct Weaver
    {
        using MachineT = std::tuple< Machines... >;
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
            std::tuple< ExMachines... > ext( exm... );
            return Extend< ExMachines... >( std::tuple_cat( _machines, ext ) );
        }

        template< typename... ExMachines >
        auto prepend( ExMachines... exm )
        {
            std::tuple< ExMachines... > ext( exm... );
            return Weaver< TQ, ExMachines..., Machines... >( std::tuple_cat( ext, _machines ) );
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

        template< typename... F >
        auto prepend_f( F... fs )
        {
            return prepend( Lambda< TQ, F >( fs ) ... );
        }

        Weaver( MachineT mt ) : _machines( mt ) {}
        Weaver() = default;

        template< typename T >
        T &machine()
        {
            return std::get< T >( _machines );
        }

        template< typename M, typename T >
        auto run_on( M &m, T &t ) -> decltype( m.run( _queue.template view< typename M::tq >(), t ) )
        {
            return m.run( _queue.template view< typename M::tq >(), t );
        }

        template< typename M, typename T >
        auto run_on( M &m, T &t ) -> decltype( m.run( t ) )
        {
            return m.run( t );
        }

        template< typename M >
        auto run_on( M &, brq::fallback )
            -> decltype( _queue.template view< typename M::tq >(), void( 0 ) )
        {}

        template< int i = 0, typename T >
        void process_task( T &t )
        {
            if constexpr ( i < std::tuple_size_v< MachineT > )
                if ( t.valid() )
                {
                    run_on( std::get< i >( _machines ), t );
                    return process_task< i + 1 >( t );
                }
        }

        template< typename T, typename... Args >
        void add( Args... args )
        {
            return push< T >( _queue, args... );
        }

        void run() /* TODO parallel */
        {
            auto process = [&]( auto t ) { process_task< 0 >( t ); };
            while ( pop( _queue, process ) );
        }

        void start()
        {
            add< task::start >();
            run();
        }
    };

}

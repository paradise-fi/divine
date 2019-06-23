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

namespace divine::mc
{

    template< typename... > struct MachineSkel;

    template< typename TS, typename TQ, typename Task, typename... Tasks >
    struct MachineSkel< TS, TQ, Task, Tasks... > : MachineSkel< TS, TQ, Tasks... >
    {
        using MachineSkel< TS, TQ, Tasks... >::run;
        void run( TQ &, Task ) {}
    };

    template< typename TS, typename TQ >
    struct MachineSkel< TS, TQ > : TS
    {
        void run();
    };

    template< typename TaskStruct, typename... TaskList >
    struct TaskQueue
    {
        using TQ = TaskQueue< TaskStruct, TaskList... >;
        using Tasks = TaskStruct;

        template< typename TS >
        struct ExtendTS : TaskStruct, TS {};

        template< typename TS, typename... Extra >
        using Extend = TaskQueue< ExtendTS< TS >, Extra..., TaskList... >;

        using Queues = std::tuple< std::deque< TaskList > ... >;
        Queues _queues;

        template< typename Task, typename... Args >
        void add( Args... args )
        {
            std::get< std::deque< Task > >( _queues ).emplace_back( args... );
        }

        template< int i = 0 >
        void clear()
        {
            if constexpr ( i < std::tuple_size_v< Queues > )
            {
                std::get< i >( _queues ).clear();
                return clear< i + 1 >();
            }
        }

        template< int i = 0, typename P >
        bool process_next( P process )
        {
            if constexpr ( i < std::tuple_size_v< Queues > )
            {
                auto &q = std::get< i >( _queues );

                if ( !q.empty() )
                {
                    process( q.front() );
                    q.pop_front();
                    return true;
                }

                return process_next< i + 1 >( process );
            }
            else
                return false;
        }

        using Skel = MachineSkel< TaskStruct, TQ, TaskList... >;
    };

    template< typename TQ, typename... Machines >
    struct Weaver
    {
        using MachineT = std::tuple< Machines... >;
        MachineT _machines;
        TQ _queue;

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

        template< typename M >
        struct Ref : std::reference_wrapper< M >
        {
            using std::reference_wrapper< M >::reference_wrapper;

            template< typename T >
            void run( TQ &tq, T t )
            {
                this->get().run( tq, t );
            }
        };

        template< typename R >
        auto extend_ref( R &ref )
        {
            return extend( Ref< R >( ref ) );
        }

        template< typename F >
        struct Observer
        {
            F f;
            Observer( F f ) : f( f ) {}

            template< typename T >
            auto run( TQ &, T t )
            {
                if constexpr ( std::__invokable< F, T >::value ) // FIXME std::is_invocable_v
                    return f( t );
            }
        };

        template< typename... F >
        auto observe( F... fs )
        {
            return extend( Observer< F >( fs ) ... );
        }

        template< typename F >
        struct Lambda
        {
            F f;
            Lambda( F f ) : f( f ) {}

            template< typename T >
            auto run( TQ &tq, T t )
            {
                if constexpr ( std::__invokable< F, TQ &, T >::value ) // FIXME std::is_invocable_v
                    return f( tq, t );
            }
        };

        template< typename... F >
        auto extend_f( F... fs )
        {
            return extend( Lambda< F >( fs ) ... );
        }

        template< typename... F >
        auto prepend_f( F... fs )
        {
            return prepend( Lambda< F >( fs ) ... );
        }

        Weaver( MachineT mt ) : _machines( mt ) {}
        Weaver() = default;

        template< typename T >
        T &machine()
        {
            return std::get< T >( _machines );
        }

        template< int i = 0, typename T >
        void process_task( T t )
        {
            if constexpr ( i < std::tuple_size_v< MachineT > )
            {
                std::get< i >( _machines ).run( _queue, t );
                return process_task< i + 1 >( t );
            }
        }

        template< typename T, typename... Args >
        void add( Args... args )
        {
            return _queue.template add< T >( args... );
        }

        void run() /* TODO parallel */
        {
            auto process = [&]( auto t ) { process_task< 0 >( t ); };
            while ( _queue.template process_next< 0 >( process ) );
        }

        void start()
        {
            add< typename TQ::Tasks::Start >();
            run();
        }
    };

}

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
        int16_t  msg_from = -1;
        int16_t  msg_to; /* -1 = any, -2 = all, >= 0 specific machine */

        base( int to = -1 ) : msg_to( to ) {}
        bool valid() { return msg_to >= -2; }
        void cancel() { msg_to = -3; }
    };

    struct start : base {};
}

namespace divine::mc
{
    struct mq_block
    {
        std::atomic< mq_block * > next;
        int16_t ptr, ctr;
        static const size_t byte_size = 4096 - 2 * sizeof( ptr ) - sizeof( next );
        static const size_t last_idx = byte_size - 1;
        uint8_t data[ byte_size ];

        mq_block() : next( nullptr ), ptr( 0 ), ctr( 0 )
        {
            data[ last_idx ] = 255;
        }

        template< typename T >
        uint8_t *aligned()
        {
            void *alloc = data + ptr;
            size_t space = byte_size - ptr - ctr - 1;
            return reinterpret_cast< uint8_t * >( std::align( alignof( T ), sizeof( T ), alloc, space ) );
        }

        template< typename T >
        bool push( const T &v, int tid )
        {
            static_assert( sizeof( T ) <= byte_size - 1 );
            ASSERT( tid < 255 );

            uint8_t *alloc = aligned< T >();
            if ( !alloc )
                return false;

            data[ last_idx - ctr ] = tid;
            std::uninitialized_copy( &v, &v + 1, reinterpret_cast< T * >( alloc ) );
            ptr = alloc + sizeof( T ) - data;
            ctr ++;
            data[ last_idx - ctr ] = 255;
            return true;
        }

        bool empty()
        {
            return data[ last_idx - ctr ] == 255;
        }

        template< typename TL, typename F, int idx = 0 >
        bool pop( F f )
        {
            if constexpr ( idx == 0 )
                if ( empty() )
                    return false;

            if constexpr ( idx >= TL::size )
                UNREACHABLE( "type match failure" );
            else if ( data[ last_idx - ctr ] == idx )
            {
                using T = typename TL::template type_at< idx >;
                uint8_t *fetch = aligned< T >();
                ASSERT( fetch );
                T *v = reinterpret_cast< T * >( fetch );
                f( *v );
                ++ ctr;
                ptr = fetch + sizeof( T ) - data;
                return true;
            }
            else
                return pop< TL, F, idx + 1 >( f );
        }
    };

    template< typename TL >
    struct mq_reader /* threads: at most one reader per queue */
    {
        mq_block *to_read;
        mq_reader() : to_read( new mq_block ) {}

        template< typename F >
        bool pop( F f )
        {
            if ( to_read->empty() && !to_read->next )
                return false;
            if ( to_read->empty() )
                to_read = to_read->next;

            ASSERT( !to_read->empty() );
            return to_read->pop< TL >( f );
        }
    };

    struct mq_sink /* threads: multiple writers should be safe */
    {
        std::atomic< mq_block * > sent;

        template< typename TL >
        mq_sink( mq_reader< TL > &r ) : sent( r.to_read ) {}

        void append( mq_block *node )
        {
            mq_block *last = sent.load();
            while ( !sent.compare_exchange_weak( last, node ) );
            last->next.store( node );
        }
    };

    struct mq_buffer /* instance per sending thread */
    {
        mq_block *open;
        mq_sink &sink;

        mq_buffer( mq_sink &sink ) : open( new mq_block ), sink( sink ) {}
        ~mq_buffer()
        {
            flush( false );
            delete open; /* in case an empty block was left behind the flush */
        }

        bool flush( bool alloc = true )
        {
            if ( !open->ctr )
                return false;

            open->ptr = open->ctr = 0;
            sink.append( open );

            if ( alloc )
                open = new mq_block;
            else
                open = nullptr;
            return true;
        }
    };

    template< typename T >
    struct mq_writer
    {
        using type = T;
        mq_buffer *buffer;
        int tid;

        void push( const T &t )
        {
            while ( !buffer->open->push( t, tid ) )
                buffer->flush();
        }
    };

    template< typename T >
    using mq_type = typename T::type;

    template< typename... tasks >
    using task_queue = typename brq::cons_list_t< tasks... >::unique_t::template map_t< mq_writer >;

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
        void push( mq_writer< std::remove_reference_t< task_t > > q, task_t &&t )
        {
            t.msg_from = machine_id();
            q.push( t );
        }

        template< typename task_t >
        void reply( mq_writer< std::remove_reference_t< task_t > > q, task_t &&t, bool update_id = true )
        {
            ASSERT_LEQ( 0, _last_msg_from );
            t.msg_to = _last_msg_from;
            if ( update_id )
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
        using task_types = typename TQ::template map_t< mq_type >;

        typename task_types::template map_t< mq_writer > _writers;
        mq_reader< task_types > _reader;
        mq_sink _sink;
        mq_buffer _buffer;

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
            i = 0;
            _writers.each( [&]( auto &w ) { w.tid = i++; w.buffer = &_buffer; } );
        }

        Weaver( MachineT mt ) : _machines( mt ), _sink( _reader ), _buffer( _sink ) { init_ids(); }
        Weaver() : _sink( _reader ), _buffer( _sink ) { init_ids(); }

        template< typename T > T &machine()
        {
            return _machines.template get< T >();
        }

        template< typename M, typename T >
        auto run_on( M &m, T &t )
            -> decltype( m.run( std::declval< typename M::tq >(), t ), true )
        {
            m.prepare( t );
            m.run( _writers.template view< typename M::tq >(), t );
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
            -> decltype( _writers.template view< typename M::tq >(), false )
        {
            return false;
        }

        template< typename T, typename... Args >
        void add( Args... args )
        {
            mq_writer< T > q = _writers;
            q.push( T( args... ) );
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
                        if ( ( t.msg_to == i || t.msg_to < 0 ) && t.msg_from != i )
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

            while ( _buffer.flush() )
                while ( _reader.pop( process ) );
        }

        void start()
        {
            add< task::start >();
            run();
        }
    };

}

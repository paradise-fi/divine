// -*- C++ -*-

#include <wibble/sfinae.h>

#include <divine/bundle.h>
#include <divine/state.h>
#include <divine/storage.h>
#include <divine/threading.h>

#include <linux/unistd.h>

#ifndef DIVINE_CONTROLLER_H
#define DIVINE_CONTROLLER_H

namespace divine {
namespace controller {

namespace impl {

using namespace wibble;

template< typename T, typename Self >
struct Simple
{
    typedef typename T::Visitor Visitor;
    typedef typename T::Observer Observer;
    typedef typename T::State State;
    typedef typename T::Storage Storage;

    Config &m_config;
    Visitor m_visitor;
    Observer m_observer;
    Storage m_storage;

    Self &self() { return *static_cast< Self* >( this ); }

    Visitor &visitor() { return m_visitor; }
    Observer &observer() { return m_observer; }
    Storage &storage() { return m_storage; }
    Config &config() { return m_config; }

    State transition( State from, State to )
    {
        typename Visitor::Storage::Reference r = visitor().merge( to );
        State ret = this->visitor().localTransition( from, r.key );
        self().visitor().storage().table().unlock( r );
        return ret;
    }

    Simple( Config &c ) : m_config( c ),
                          m_visitor( self() ),
                          m_observer( self() ),
                          m_storage( self() ) {}
};

template< typename T >
struct FifoVector
{
    int m_last;
    typedef divine::Fifo< T, NoopMutex > Fifo;
    std::vector< Fifo > m_vector;

    bool empty() {
        for ( int i = 0; i < m_vector.size(); ++i ) {
            Fifo &fifo = m_vector[ (m_last + i) % m_vector.size() ];
            if ( !fifo.empty() )
                return false;
        }
        return true;
    };

    T &next() {
        for ( int i = 1; i < m_vector.size() + 1; ++i ) {
            Fifo &fifo = m_vector[ (m_last + i) % m_vector.size() ];
            if ( !fifo.empty() ) {
                m_last = (m_last + i) % m_vector.size();
                return fifo.front();
            }
        }
        assert( 0 );
        T *x = 0; return *x; // suppress warning; should never happen
    }

    void push( T t, int id ) {
        assert( id < m_vector.size() );
        m_vector[ id ].push( t );
    }

    void remove() {
        m_vector[ m_last ].pop();
    }

    void resize( int i ) { m_vector.resize( i, Fifo() ); }
    FifoVector() : m_last( 0 ) {}
};

template< typename T, typename Self >
struct Thread : Simple< T, Self >, wibble::sys::Thread,
    TerminationConsole::Pluggable
{
    typedef typename T::State State;
    typedef typename T::Visitor Visitor;
    typedef threads::Pack< Self > Pack;

    bool m_terminate, m_terminateRequested;
    int m_id;
    Pack *m_pack;

    // already cache-line protected, should be enough for the
    // controller object as well
    FifoVector< std::pair< State, State > > m_work;

    void setPack( Pack &p ) {
        m_pack = &p;
        m_id = pack().id( self() );
        m_work.resize( pack().workerCount() );
    }

    Pack &pack() { return *m_pack; }

    int id() {
        return m_id;
    }

    Self &self() { return *static_cast< Self* >( this ); }

    void *main() {
        assert( plugged() );
        int idled = 0;
        MutexLock lock( plug()->mutex );
        plug()->started();
        while ( !m_terminate ) {
            assert( !m_terminate );
            int spin = 50;
            while ( !checkForWork() && !m_terminate ) {
                if ( spin > 0 ) {
                    if ( spin > 10 )
                        sched_yield();
                    else
                        usleep( (10 - spin) * 5000 );
                    -- spin;
                } else {
                    ++ idled;
                    plug()->idle( lock );
                    spin = 50;
                }
            }
            self().stateWork();
            if ( m_terminateRequested ) {
                plug()->terminate();
                plug()->idle( lock );
            }
        }
        m_terminate = false;
        self().parallelTermination();
        return 0;
    }

    void parallelTermination()
    {
        self().observer().parallelTermination();
    }

    void serialTermination()
    {
        self().observer().serialTermination();
    }

    bool checkForWork() {
        return !m_work.empty();
    }

    void terminate()
    {
        assert( plugged() );
        m_terminate = true;
        plug()->wakeup();
    }

    void requestTermination()
    {
        m_terminateRequested = true;
    }

    void deallocationRequest( std::pair< State, State > t )
    {
        self().storage().deallocate( t.second );
    }

    State localTransition( State from, State to )
    {
        typename Visitor::Storage::Reference r = self().visitor().merge( to );
        if ( to.pointer() != r.key.pointer() ) {
            self().deallocationRequest( std::make_pair( from, to ) );
        }
        State ret = this->visitor().localTransition( from, r.key );
        self().visitor().storage().table().unlock( r );
        return ret;
    }

    State transition( State from, State to )
    {
        assert( to.valid() );
        int tgt = self().target( from, to );
        if ( tgt == self().id() ) {
            return localTransition( from, to );
        } else {
            if ( tgt != -1 ) {
                to.clearDeleteFlag(); // do not delete this...
                pack().worker( tgt ).queue( std::make_pair( from, to ),
                                            self().id() );
            }
            return State(); // do not process this transition here
        }
    }

    void queue( State s ) {
        assert( s.valid() );
        assert( !plug()->console->isRunning() );
        self().queue( std::make_pair( State(), s ), 0 );
    }

    void queue( std::pair< State, State > t, int id ) {
        assert( t.second.valid() );
        m_work.push( t, id );
    }

    Thread( Config &c ) : Simple< T, Self >( c ),
                          m_terminate( false ),
                          m_terminateRequested( false ),
                          m_id( -1 )
    {
        m_work.resize( 1 );
    }
};

template< typename T, typename Self >
struct Partition : Thread< T, Self >
{
    typedef typename T::State State;
    typedef typename T::Storage Storage;
    typedef threads::Pack< Self > Pack;

    Self &self() { return *static_cast< Self* >( this ); }

    int target( State from, State to )
    {
        return self().owner( to );
    }

    void stateWork()
    {
        while ( this->checkForWork() ) {
            std::pair< State, State > t = this->m_work.next();
            assert( t.second.valid() );
            assert( self().owner( t.second ) == self().id() );
            State to = self().transition( t.first, t.second );
            if ( to.valid() )
                self().visitor().visit();
            this->m_work.remove();
            if ( this->m_terminateRequested )
                return;
        }
    }

    template< typename S >
    typename EnableIf< storage::IsStealing< S > >::T
    _deallocationRequest( std::pair< State, State > t )
    {
        self().storage().steal( t.second );
        return Unit();
    }

    template< typename S >
    typename EnableIf< TNot< storage::IsStealing< S > > >::T
    _deallocationRequest( std::pair< State, State > t )
    {
        self().storage().deallocate( t.second );
        return Unit();
    }

    void deallocationRequest( std::pair< State, State > t )
    {
        _deallocationRequest< Storage >( t );
    }

    int owner( State st ) {
        return (st.hash( 12 ) % 13001) % self().pack().workerCount();
    }

    Partition( Config &c ) : Thread< T, Self >( c )
    {}
};

template< typename T, typename Self >
struct Shared : Thread< T, Self >
{
    typedef typename T::State State;
    int m_counter;

    Self &self() { return *static_cast< Self* >( this ); }

    // all outside states go to first worker (FIXME?)
    int owner( State st )
    {
        return 0;
    }

    int target( State from, State to )
    {
        ++ m_counter;
        if ( m_counter % self().config().handoff() == 0 ) {
            return (m_counter / self().config().handoff())
                % self().pack().workerCount();
        }
        return self().id();
    }

    void stateWork()
    {
        while ( this->checkForWork() ) {
            std::pair< State, State > t = this->m_work.next();
            assert( t.second.valid() );
            State to = self().transition( t.first, t.second );
            if ( to.valid() )
                self().visitor().visit( to );
            this->m_work.remove();
        }
    }

    Shared( Config &c ) : Thread< T, Self >( c ), m_counter( 0 )
    {}
};

template< typename T, typename Self >
struct Handoff : Shared< T, Self >
{
    typedef typename T::State State;
    int target( State from, State to )
    {
        if ( this->self().visitor().height() >= this->self().config().handoff() )
            return (this->self().id() + 1) % this->self().pack().workerCount();
        return this->self().id();
    }

    Handoff( Config &c ) : Shared< T, Self >( c )
    {}
};

template< typename T, typename Self >
struct Slave : Simple< T, Self >, wibble::sys::Thread
{
    typedef typename T::State State;
    wibble::sys::Condition cond;
    bool m_terminate;

    Fifo< State > m_workFifo;
    void queue( State s ) {
        m_workFifo.push( s );
    }

    void *main() {
        while ( !m_terminate || !m_workFifo.empty() ) {
            if ( m_workFifo.empty() ) {
                Mutex foo;
                MutexLock foobar( foo );
                cond.wait( foobar );
            }
            while ( !m_workFifo.empty() ) {
                this->visitor().visit( m_workFifo.front() );
                m_workFifo.pop();
            }
        }
        m_terminate = false;
        return 0;
    }

    void terminate() {
        m_terminate = true;
        cond.signal();
    }

    Slave( Config &c ) : Simple< T, Self >( c ) {
        m_terminate = false;
    }
};

}

typedef FinalizeController< impl::Simple > Simple;
typedef FinalizeController< impl::Thread > Thread;
typedef FinalizeController< impl::Shared > Shared;
typedef FinalizeController< impl::Partition > Partition;
typedef FinalizeController< impl::Handoff > Handoff;
typedef FinalizeController< impl::Slave > Slave;

}

}

#endif

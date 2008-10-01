// -*- C++ -*-
#include <wibble/sys/mutex.h>
#include <wibble/sys/thread.h>
#include <divine/config.h>
#include <list>
#include <map>
#include <vector>
#include <deque>
#include <cassert>

#ifndef DIVINE_THREADING_H
#define DIVINE_THREADING_H

const int cacheLine = 64;

namespace divine {
// fixme?
using namespace wibble::sys;

struct NoopMutex {
    void lock() {}
    void unlock() {}
};

// make the node a page-sized object by default; hopefully, the
// allocator will figure to page-align the object which gives optimal
// cache usage
template< typename T, typename WM = Mutex,
          int NodeSize = (4096 - cacheLine - sizeof(int)
                          - sizeof(void*)) / sizeof(T) >
struct Fifo {
protected:
    WM writeMutex;
    // the Node layout puts read and write counters far apart to avoid
    // them sharing a cache line, since they are always written from
    // different threads
    struct Node {
        int read;
        char padding[ cacheLine - sizeof(int) ];
        T buffer[ NodeSize ];
        // TODO? we can save some multiplication instructions by using
        // pointers/iterators here, where we can work with addition
        // instead which is very slightly cheaper
        volatile int write;
        Node *next;
        Node() {
            read = write = 0;
            next = 0;
        }
    };

    // pad the fifo object to ensure that head/tail pointers never
    // share a cache line with anyone else
    char _padding1[cacheLine-2*sizeof(Node*)];
    Node *head, *freetail;
    char _padding2[cacheLine-2*sizeof(Node*)];
    Node *tail, *freehead;
    char _padding3[cacheLine-2*sizeof(Node*)];

public:
    Fifo() {
        head = tail = new Node();
        freehead = freetail = new Node();
        assert( empty() );
    }

    // copying a fifo is not allowed
    Fifo( const Fifo &f ) {
        head = tail = new Node();
        freehead = freetail = new Node();
        assert( empty() );
    }

    virtual ~Fifo() {} // TODO free up stuff here

    void push( const T& x ) {
        Node *t;
        writeMutex.lock();
        if ( tail->write == NodeSize ) {
            if ( freehead != freetail ) {
                // grab a node for recycling
                t = freehead;
                assert( freehead->next != 0 );
                freehead = freehead->next;

                // clear it
                t->read = t->write = 0;
                t->next = 0;

                // dump the rest of the freelist
                while ( freehead != freetail ) {
                    Node *next = freehead->next;
                    assert( next != 0 );
                    delete freehead;
                    freehead = next;
                }

                assert( freehead == freetail );
            } else {
                t = new Node();
            }
        } else
            t = tail;

        t->buffer[ t->write ] = x;
        ++ t->write;

        if ( tail != t ) {
            tail->next = t;
            tail = t;
        }
        writeMutex.unlock();
    }

    bool empty() {
        return head == tail && head->read >= head->write;
    }

    void dropHead() {
        Node *old = head;
        head = head->next;
        assert( head );
        old->next = 0;
        freetail->next = old;
        freetail = old;
    }

    void pop() {
        assert( !empty() );
        ++ head->read;
        if ( head->read == NodeSize ) {
            if ( head->next != 0 ) {
                dropHead();
            }
        }
    }

    T &front() {
        assert( head );
        assert( !empty() );
        // last pop could have left us with empty queue exactly at an
        // edge of a block, which leaves head->read == NodeSize
        if ( head->read == NodeSize ) {
            dropHead();
        }
        return head->buffer[ head->read ];
    }
};

struct TerminationConsole : wibble::sys::Thread
{
    struct Plug;

    struct Pluggable {
        Plug *m_plug;

        virtual bool checkForWork() = 0;

        void plug( TerminationConsole &c ) {
            m_plug = &c.plug();
            m_plug->pluggable = this;
        }

        bool plugged() {
            return m_plug;
        }

        Plug *plug() { return m_plug; }

        Pluggable() : m_plug( 0 ) {}
    };

    struct Plug {
        Mutex mutex;
        Condition cond;
        Pluggable *pluggable;
        TerminationConsole *console;

        void started() {
            console->m_started = true;
        }

        void idle( MutexLock &lock ) {
            console->wakeup( this );
            // the race condition here is countered by
            // TerminationConsole, which waits for our mutex
            // (it may otherwise assume we are busy and deadlock the
            // system if we were last to become idle)
            cond.wait( lock );
        }

        void wakeup() {
            mutex.lock();
            cond.signal();
            mutex.unlock();
        }

        void terminate() {
            console->m_terminate = true;
        }

        Plug( TerminationConsole &tc ) : console( &tc ) {}
    };

    typedef std::list< Plug > PlugList;

    // mutex and condition are already cache-line protected, so we
    // stuff all the rest of the data in between
    Mutex m_wakeupMutex;
    PlugList m_plugs;
    Plug *m_wakeup;
    bool m_started;
    bool m_terminated, m_terminate;
    bool m_fifoOnly;
    Condition m_wakeupCondition;

    void reset() {
        m_fifoOnly = false;
        m_started = false;
        m_terminated = false;
        m_terminate = false;
        m_wakeup = 0;
    }

    Plug &plug() {
        m_plugs.push_back( Plug( *this ) );
        return m_plugs.back();
    }

    bool isBusy() {
        std::vector< bool > pwned;
        bool busy = false;

        PlugList::iterator i;
        int j;
        for ( j = 0, i = m_plugs.begin(); i != m_plugs.end(); ++i, ++j ) {
            if ( !i->mutex.trylock() ) {
                // we have to wait for the wakeup thread's lock,
                // since it may not be asleep yet and we lose
                if ( &*i == m_wakeup ) {
                    i->mutex.lock();
                    pwned.push_back( true );
                } else {
                    pwned.push_back( false );
                    busy = true;
                }
            } else
                pwned.push_back( true );
        }

        // we hold here all that we could get hold of
        for ( j = 0, i = m_plugs.begin(); i != m_plugs.end(); ++i, ++j ) {
            if ( pwned[ j ] ) {
                if ( i->pluggable->checkForWork() ) {
                    busy = true;
                    i->cond.signal();
                }
            }
        }

        for ( j = 0, i = m_plugs.begin(); i != m_plugs.end(); ++i, ++j ) {
            if ( pwned[ j ] ) {
                i->mutex.unlock();
            }
        }

        return busy;
    }

    void checkFifos() {
        PlugList::iterator i;
        int j;
        for ( j = 0, i = m_plugs.begin(); i != m_plugs.end(); ++i, ++j ) {
            if ( i->pluggable->checkForWork() ) {
                i->cond.signal();
            }
        }
    }

    void wait() {
        MutexLock wm( m_wakeupMutex );
        while( true ) {

            if ( !m_started ) {
                m_wakeupCondition.wait( wm );
            }

            if ( m_fifoOnly )
                checkFifos();
            else {
                if ( !isBusy() || m_terminate ) {
                    m_terminated = true;
                    m_started = false;
                    return;
                }
            }

            // TODO the timeout here may be replaced by
            // unconditionally signalling a FIFO owner once the fifo
            // contains say 50 or more items, then every next full 50
            // items
            m_wakeup = 0;
            if ( m_wakeupCondition.wait( wm ), 1 ) {
                m_fifoOnly = false;
            } else
                m_fifoOnly = true;
        }

        m_started = false;
    }

    bool isRunning() {
        return m_started;
    }

    void *main() { wait(); return 0; }

    void wakeup( Plug *w = 0 ) {
        m_wakeupMutex.lock();
        m_wakeup = w;
        m_wakeupCondition.signal();
        m_wakeupMutex.unlock();
    }

    TerminationConsole() : m_started( false ) {}

};

namespace threads {

template< typename _Worker >
struct Pack : Thread
{
    typedef _Worker Worker;
    typedef typename Worker::State State;

    Config &m_config;
    std::vector< Worker * > m_workers;
    std::map< Worker *, int > m_ids;
    int m_threads;
    bool m_terminated;

    TerminationConsole m_tc;

    Config &config() { return m_config; }

    Pack( Config &c ) : m_config( c ), m_threads( 0 ) {}

    int id( Worker &w ) {
        return m_ids[ &w ];
    }

    void setThreadCount( int size ) {
        m_threads = size;
    }

    void initialize()
    {
        if ( !m_threads )
            m_threads = config().maxThreadCount() - config().threadCount();
        for ( int i = 0; i < m_threads; ++ i ) {
            m_workers.push_back( new Worker( m_config ) );
            m_ids[ m_workers.back() ] = i;
            m_workers.back()->setPack( *this );
            m_workers.back()->plug( m_tc );
        }
    }

    void run() {
        m_tc.reset();
        for ( int i = 0; i < m_threads; ++i ) {
            worker( i ).start();
        }
    }

    void *main() {
        monitor();
        return 0;
    }

    void asyncMonitor() {
        start();
    }

    // termination detection
    void monitor() {
        m_tc.wait();
        termination();
    }

    void reset() {
        m_terminated = false;
        m_tc.reset();
        for ( int i = 0; i < m_threads; ++ i ) {
            worker( i ).observer().reset();
        }
    }

    void blockingVisit( State st = State() )
    {
        if ( !st.valid() )
            st = anyWorker().visitor().sys.initial();
        worker( owner( st ) ).queue( st );
        run();
        monitor();
    }

    void blockingRun()
    {
        run();
        monitor();
    }

    void terminate() {
        for ( int i = 0; i < m_threads; ++i ) {
            worker( i ).requestTermination();
        }
    }

    void termination() {
        for ( int i = 0; i < m_threads; ++i ) {
            worker( i ).terminate();
            worker( i ).join();
            config().threadFinished( worker( i ) );
        }
        for ( int i = 0; i < m_threads; ++i ) {
            worker( i ).serialTermination();
        }
        m_terminated = true;
    }

    Worker &worker( int id )
    {
        return *m_workers[ id ];
    }

    int owner( typename Worker::State st ) {
        return anyWorker().owner( st );
    }

    Worker &anyWorker()
    {
        assert( m_workers.size() >= 1 );
        return worker( 0 );
    }

    int workerCount() { return m_threads; /* m_workers.size(); */ }
    bool hasTerminated() { return m_terminated; }

};

}

#define PUSHED (State::FirstUserFlag)
#define SEEN (State::FirstUserFlag << 1)

}

#endif

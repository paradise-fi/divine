// -*- C++ -*-

#ifndef DIVINE_DISKFIFO_H
#define DIVINE_DISKFIFO_H

#include <fstream>
#include <wibble/sys/mutex.h>
#include <divine/toolkit/fifo.h>
#include <divine/toolkit/pool.h>
#include <divine/toolkit/bitstream.h>

namespace divine {

/**
 * A simple queue (First-In, First-Out). Concurrent access to the ends of the
 * queue is supported -- a thread may write to the queue while another is
 * reading. Concurrent access to a single end is, however, not supported.
 *
 * The NodeSize parameter defines a size of single block of objects. By
 * default, we make the node a page-sized object -- this seems to work well in
 * practice. We rely on the allocator to align the allocated blocks reasonably
 * to give good cache usage.
 */
template< typename T,
          int NodeSize = (32 * 4096 - cacheLine - sizeof(int)
                          - 2*sizeof(void*)) / sizeof(T) >
struct DiskFifo {
protected:
    // the Node layout puts read and write counters far apart to avoid
    // them sharing a cache line, since they are always written from
    // different threads
    struct Node {
        T *read;
        char padding[ cacheLine - sizeof(int) ];
        T buffer[ NodeSize ];
        T * volatile write;
        Node *next;
        unsigned int id;
        Node( unsigned int _id ) : id( _id ) {
            read = write = buffer;
            next = 0;
        }
    };

    // pad the fifo object to ensure that head/tail pointers never
    // share a cache line with anyone else
    char _padding1[ cacheLine - 2*sizeof(Node*) ];
    Node *head;
    char _padding2[ cacheLine - 2*sizeof(Node*) ];
    Node * volatile mid;
    Node * volatile tail;
    char _padding3[ cacheLine - 2*sizeof(Node*) ];
    std::streampos rpos = 0, wpos = 0;  // positions of next read and write operations
    int rfid = 0, wfid = 0; // ids of read and write files
    std::fstream rfile, wfile; //read and write files
    wibble::sys::Mutex faccess;
    bool saving = false;

public:
    Pool pool;

    DiskFifo() {
        head = tail = new Node( 0 );
        mid = NULL;
        assert( empty() );
    }

    // copying a fifo is not allowed
    DiskFifo( const DiskFifo & ) {
        head = tail = new Node( 0 );
        mid = NULL;
        assert( empty() );
    }

    ~DiskFifo() {
        closeFiles();
        while ( head->next ) {
            Node *next = head->next;
            assert( next != 0 );
            delete head;
            head = next;
        }
        delete head;
        if ( mid ) {
            while ( mid->next ) {
                Node *next = mid->next;
                assert( next != 0 );
                delete mid;
                mid = next;
            }
            delete mid;
        }
    }

    void enableSaving( bool enable = true ) {
        saving = enable;
    }

    bool isSavingEnabled() const {
        return saving;
    }

    void push( const T&x ) {
        Node *t;
        if ( tail->write == tail->buffer + NodeSize )
            t = new Node( tail->id + 1 );
        else
            t = tail;

        *t->write = x;
        ++ t->write;
        __sync_synchronize();

        if ( tail != t ) {
            tail->next = t;
            __sync_synchronize();
            tail = t;
            if ( saving )
                saveNodes();
        }
    }

    bool empty() {
        return head == tail && head->read >= head->write;
    }

    int size() {
        int size = 0;
        size += head->write - head->read;
        if ( head != tail ) {
            size += ( tail->id - head->id - 1 ) * NodeSize;
            size += tail->write - tail->read;
        }
        return size;
    }

    void dropHead() {
        if ( !head->next )
            loadNodes();
        Node *old = head;
        head = head->next;
        assert( head );
        assert( old->id + 1 == head->id );
        delete old;
    }

    void pop() {
        assert( !empty() );
        ++ head->read;
        if ( head->read == head->buffer + NodeSize ) {
            if ( head != tail ) {
                dropHead();
            }
        }
        // the following can happen when head == tail even though head->read
        // has reached NodeSize, *and* no front() has been called in the meantime
        if ( head->read > head->buffer + NodeSize ) {
            dropHead();
            pop();
        }
    }

    T &front( bool wait = false ) {
        while ( wait && empty() ) ;
        assert( head );
        assert( !empty() );
        // last pop could have left us with empty queue exactly at an
        // edge of a block, which leaves head->read == NodeSize
        if ( head->read == head->buffer + NodeSize ) {
            dropHead();
        }
        return *head->read;
    }

private:
    std::string fileName( int id ) {
		static std::string path;
		if ( path.empty() ) {
			path = "/tmp/";
			for ( const char *env : {"TMPDIR", "TEMP", "TMP"} ) {
				const char* p = getenv( env );
				if ( p ) {
					path = p;
					break;
				}
			}
		}
        std::stringstream ss;
        ss << "fifo" << this << '_' << id << ".tmp";
        return wibble::str::joinpath( path, ss.str() );
    }

    void closeFiles() {
        rfile.close();
        for ( ; rfid < wfid; rfid++ ) {
            remove( fileName( rfid ).c_str() );
        }
		if ( wfile.is_open() ) {
        	wfile.close();
			remove( fileName( wfid ).c_str() );
		}
    }

    void saveNodes() {
        const unsigned int COUNT = 1;
        // write block of nodes into file if FIFO is long enough
        if ( mid ? ( tail->id - mid->id > COUNT ) : ( tail->id - head->id > COUNT+1 ) ) {
            // avoid file access from multiple threads
            if ( !faccess.trylock() ) return; // writing can wait if reading is in progress

            if ( !mid ) {   // find the middle
                Node *n = head;
                for ( unsigned int i = 0; i < COUNT; i++ )
                    n = n->next;
                assert( n != tail );
                mid = n->next;
                n->next = NULL; // break the chain of nexts
            }

            // save everything from mid to tail
            while ( mid->next ) {
                Node *old = mid;
                if ( !write( old ) ) break;
                mid = old->next;
                delete old;
            }
            faccess.unlock();
        }
    }

    void loadNodes() {
        // avoid file access from multiple threads
        faccess.lock();
        // try to read up to COUNT nodes
        const unsigned int COUNT = 1;
        Node *n = head;
        for ( unsigned int i = 0; i < COUNT; i++ ) {
            Node *load = new Node( n->id + 1 );
            load->write = load->buffer + NodeSize;
            // we may have to reconnect the list if there is nothing to read
            if ( !read( load ) ) {
                assert( mid && load->id == mid->id );
                delete load;
                load = mid;
                mid = NULL;
                i = COUNT;  // exit after this iteration
            }
            n->next = load;
            n = load;
        }
        faccess.unlock();
    }

    bool write( Node *n ) {
		// files exceeding this size are split (0 disables this)
		const std::streampos MAXSIZE = 2000 * 1024 * 1024;

        if ( !wfile.is_open() ) {
            wfile.open( fileName( wfid ).c_str(),
                std::ios_base::out | std::ios_base::binary | std::ios_base::trunc );
            if ( !wfile.is_open() ) {
                std::cerr << "WARNING: Opening file for fifo failed" << std::endl;
                return false;
            }
            wpos = 0;
        }

        bitblock bb( pool );
        bb.bits.reserve( NodeSize * 2 ); // avoids some reallocation
        bb.push( 0 );

        for ( T *p = n->buffer; p != n->buffer + NodeSize; p++ ) {
            bb << *p;
        }
        bb.front() = bb.size() - 1; // store length at the beginning

        wfile.seekp( wpos );
        unsigned int toWrite = bb.size() * 4;
        wfile.write( reinterpret_cast< char* >( &bb.front() ), toWrite );
        if ( !wfile.good() ) {
             std::cerr << "WARNING: Writing fifo to disk failed" << std::endl;
             return false;
        }
        wpos += toWrite;
        if ( MAXSIZE && wpos > MAXSIZE ) {
            wfile.close();
        }

        // release nodes
        for ( T *p = n->buffer; p != n->buffer + NodeSize; p++ ) {
            release( *p, &pool );
        }

        return true;
    }

    bool read( Node *n ) {
        uint32_t length = 0;
        do {
            if ( rfid == wfid && wpos == rpos )
                return false; // nothing to read
            if ( rfid == wfid ) { // prevent reading and writing to the same file
                wfile.close();
                wfid++;
            }

            if ( !rfile.is_open() ) {
                rfile.open( fileName( rfid ).c_str(),
                    std::ios_base::in | std::ios_base::binary );
                if ( !rfile.is_open() ) {
                    std::cerr << "ERROR: Reading FAILED, aborting" << std::endl;
                    closeFiles();
                    abort();
                }
                rpos = 0;
            }

            // read the length;
            rfile.seekg( rpos );
            rfile.read( reinterpret_cast< char* >( &length ), sizeof( length ) );
            if ( !rfile.good() || length == 0 ) { // reached the end of file, try the next one
                rfile.close();
                remove( fileName( rfid ).c_str() );
                rfid++;
            }
        } while ( length == 0 );

        bitblock bb( pool );
        bb.bits.resize( length );
        rfile.read( reinterpret_cast< char* >( &bb.front() ), length * 4 );
        rpos += (length + 1) * 4;

        for ( T *p = n->buffer; p != n->buffer + NodeSize; p++ ) {
            bb >> *p;
        }
        assert( bb.size() == 0 );
        return true;
    }
};

template< std::size_t I = 0, typename... Tp >
inline typename std::enable_if< (I == sizeof...(Tp)), void >::type
release( std::tuple< Tp... >&, Pool* ) {}

template< std::size_t I = 0, typename... Tp >
inline typename std::enable_if< (I < sizeof...(Tp)), void >::type
release( std::tuple< Tp... >& t, Pool* p ) {
    release( std::get< I >( t ), p );
    release< I + 1, Tp... >( t, p );
}

template< typename T >
inline void release( T, Pool* ) {
}

template<>
inline void release( Blob b, Pool* p ) {
    p->free( b );
}

}

#endif

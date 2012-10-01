#ifndef DIVINE_FILEFIFO_H
#define DIVINE_FILEFIFO_H

#include <wibble/sys/mutex.h>
#include <divine/toolkit/fifo.h>
#include <divine/toolkit/pool.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

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
 * 
 * This implementation tries to reduce memory usage by saving parts of the queue
 * into files. It assumes T is std::pair of Blobs, where the second one is not
 * referenced anywhere else and can be safely freed.
 */
template< typename T,
          int NodeSize = (32 * 4096 - cacheLine - sizeof(int)
                          - 2*sizeof(void*)) / sizeof(T) >
struct FileFifo {
protected:
    // the Node layout puts read and write counters far apart to avoid
    // them sharing a cache line, since they are always written from
    // different threads
    struct Node {
        T *read;
        char padding[ cacheLine - sizeof(void*) ];
        T buffer[ NodeSize ];
        T * volatile write;
        Node *next;
        unsigned int id;
        Node() {
            read = write = buffer;
            next = 0;
            id = 0;
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
    std::streampos rpos, wpos;
    int rfile, wfile;
    std::fstream  files[ 2 ];
    Pool *dpool, *apool;
    wibble::sys::Mutex faccess;

    // Nodes are loaded and saved in blocks whose size is given by following variable.
    // Increasing it leads to O/I occuring less often and in bigger batches.
    // Memory consumption is also given by this variable because no more than 2*block Nodes
    // will be kept in memory.
    unsigned int block;

public:
    FileFifo() {
        head = tail = new Node();
        mid = NULL;
        assert( empty() );
        block = 0;
        rfile = wfile = 0;
        wpos = rpos = 0;
    }

    // copying fifo is not allowed
    FileFifo( const FileFifo & ) {
        head = tail = new Node();
        mid = NULL;
        assert( empty() );
        block = 0;
        rfile = wfile = 0;
        wpos = rpos = 0;
    }

    FileFifo& operator=( const FileFifo & ) {
        return *this;
    }

    ~FileFifo() {
        closeFiles();
        while ( head->next ) {
            Node *next = head->next;
            delete head;
            head = next;
        }
        delete head;
        if ( mid ) {
            while ( mid->next ) {
                Node *next = mid->next;
                delete mid;
                mid = next;
            }
            delete mid;
        }
    }

    /*
    blockSize adjusts number of blocks loaded and saved at once (0 disables using files)
    pd is used to deallocate Blobs when writing
    pa is used to allocate them again when reading
    pd and pa are ignored if set to null, which means each of them can be set by separate call
    */
    void setup( int blockSize, Pool* pd, Pool* pa ) {
        if ( pd ) dpool = pd;
        if ( pa ) apool = pa;
        block = blockSize;
    }

    void push( const T&x ) {
        Node *t;
        if ( tail->write == tail->buffer + NodeSize ) {
            t = new Node();
            t->id = tail->id + 1;

            if ( block ) {
                saveNodes();
            }
        } else {
            t = tail;
        }

        *t->write = x;
        ++ t->write;

        if ( tail != t ) {
            tail->next = t;
            tail = t;
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
            if ( head != tail )
                dropHead();
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

protected:
    std::string fileName( int id ) {
        std::stringstream ss;
#ifdef POSIX
        ss << "/tmp/";
#elif defined _WIN32
        ss << "%TEMP%\\";
#endif
        ss << "fifo" << this << "_" << id << ".tmp";
        return ss.str();
    }

    void closeFiles() {
        for ( int i = 0; i < 2; i++ ) {
            if ( files[ i ].is_open() )
                files[ i ].close();
            remove( fileName( i ).c_str() );
        }
    }

    static void push32_ptr( std::vector< uint32_t > &vec, void *ptr ) {
        vec.push_back( uint32_t( uintptr_t( ptr ) ) );
        if ( sizeof( ptr ) > 4 ) {
            vec.push_back( uint32_t( uint64_t( ptr ) >> 32 ) );
        }
    }

    static char* read32_ptr( std::vector< uint32_t >::const_iterator &it ) {
        uintptr_t ptr = *it++;
        if ( sizeof( ptr ) > 4 ) {
            ptr |= uint64_t( *it++ ) << 32;
        }
        return (char*) ptr;
    }

    void saveNodes() {
        // write block of nodes into file if FIFO is long enough
        if ( ( mid && ( tail->id - mid->id > block ) ) ||
                ( !mid && ( tail->id - head->id > 2*block ) ) ) {
            // avoid file access from multiple threads
            if ( faccess.trylock() ) {  // writing can wait if reading is in progress
                if ( !mid ) {   // find the middle
                    Node *n = head;
                    for ( unsigned int i = 0; i < block; i++ )
                        n = n->next;
                    mid = n->next;
                    n->next = NULL; // break the chain of nexts
                }

                while ( mid->next ) {
                    Node *old = mid;
                    if ( !write( old ) ) break;
                    mid = old->next;
                    delete old;
                }
                faccess.unlock();
            }
        }
    }

    void loadNodes() {
        // avoid file access from multiple threads
        faccess.lock();
        // try to read up to BLOCK nodes
        Node *n = head;
        unsigned int readBlocks = std::max( block, 1u );
        for ( unsigned int i = 0; i < readBlocks; i++ ) {
            Node *load = new Node();
            load->write = load->buffer + NodeSize;
            load->id = n->id + 1;
            // we may have to reconnect the list if there is nothing to read
            if ( !read( load ) ) {
                assert( mid && load->id == mid->id );
                delete load;
                load = mid;
                mid = NULL;
                i = readBlocks;  // exit after this iteration
            }
            n->next = load;
            n = load;
        }
        faccess.unlock();
    }

    bool write( Node *n ) {
        if ( wfile == rfile && wpos != 0 ) {    // switch to the other file
            wfile = !wfile;
            wpos = 0;
        }
        std::fstream &file = files[ wfile ];

        if ( !file.is_open() ) {    // open file if needed
            file.open( fileName( wfile ).c_str(),
                std::ios_base::in | std::ios_base::out | std::ios_base::binary | std::ios_base::trunc );
            if ( !file.is_open() ) {
                std::cerr << "WARNING: Opening file for fifo failed" << std::endl;
                block = 0; // disable using files
                return false;
            }
        }

        static size_t reserve = NodeSize;   // to prevent unnecessary reallocation
        std::vector< uint32_t > bfr;
        bfr.reserve( reserve );
        bfr.push_back( 0 ); // reserve space for length

        for ( T *p = n->buffer; p != n->buffer + NodeSize; p++ ) {
            p->first.write32( std::back_inserter( bfr ) );
            p->second.write32( std::back_inserter( bfr ) );
            // defer releasing items after successful write
        }

        bfr.front() = bfr.size() - 1;   // store length
        bfr.push_back( 0 ); // place zero into length field of next block to prevent reading that block

        file.seekp( wpos );
        file.write( (char*) &bfr.front(), bfr.size()*4 );
        wpos += (bfr.size() - 1) * 4;
        if ( !file.good() ) {
             std::cerr << "WARNING: Writing fifo to disk failed" << std::endl;
             return false;
        }
        reserve = std::max( reserve, bfr.size() );

        // actually release states
        for ( T *p = n->buffer; p != n->buffer + NodeSize; p++ ) {
            p->first.free( *dpool );
            p->second.free( *dpool );
        }

        return true;
    }

    bool read( Node *n ) {
        if ( rfile == wfile && rpos == wpos )
            return false;   // nothing to read
        T *p = n->buffer;

        std::fstream &file = files[ rfile ];
        file.seekg( rpos );
        uint32_t length = 0;
        file.read( (char*) &length, sizeof( length ) );
        if ( !file.good() ) {
            std::cerr << "ERROR: Reading FAILED, aborting" << std::endl;
            closeFiles();
            abort();
        }
        if ( length == 0 ) {   // end was reached, try the other file
            files[ rfile ].close(); // we can close the  file to save space
            rfile = !rfile;
            rpos = 0;
            return read( n );
        }
        std::vector< uint32_t > bfr( length );
        file.read( (char*) &bfr.front(), length*4 );
        rpos += (length + 1) * 4;

        std::vector< uint32_t >::const_iterator it = bfr.begin();
        while ( it != bfr.end() ) {
            it = p->first.read32( apool, it );
            it = p->second.read32( apool, it );
            p++;
        }
        assert( p == n->buffer + NodeSize );

        return true;
    }
};

}

#endif

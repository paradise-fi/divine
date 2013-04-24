#include <divine/toolkit/diskfifo.h>

using namespace divine;

struct TestDiskFifo {
	Test testInt() {
		// choose small node size to force saving
		DiskFifo< int, 100 > fifo;
		fifo.enableSaving( true );
		const int MAX = 10 * 1024;
		for ( int i = 0; i < MAX; i++ ) {
			fifo.push( i );
		}
		for ( int i = 0; i < MAX; i++ ) {
			assert_eq( fifo.front(), i );
			fifo.pop();
		}
		assert( fifo.empty() );
	}

	Test testBlob() {
		Pool pool;
		// choose small node size to force saving
		DiskFifo< Blob, 100 > fifo;
		fifo.enableSaving( true );
		const int MAX = 10 * 1024;
		for ( int i = 0; i < MAX; i++ ) {
            Blob b = pool.allocate( sizeof( int ) );
            pool.get< int >( b ) = i;
			fifo.push( b );
		}
		for ( int i = 0; i < MAX; i++ ) {
			assert_eq( pool.get< int >( fifo.front() ), i );
			pool.free( fifo.front() );
			fifo.pop();
		}
		assert( fifo.empty() );
	}
};

// -*- C++ -*- (c) 2012 Petr Rockai

#include <divine/llvm/program.h>
#include <divine/toolkit/lens.h>
#include <divine/graph/allocator.h>

#ifndef DIVINE_LLVM_MACHINE_H
#define DIVINE_LLVM_MACHINE_H

namespace divine {
namespace llvm {

struct MachineState
{
    struct StateAddress : lens::LinearAddress
    {
        ProgramInfo *_info;

        StateAddress( StateAddress base, int index, int offset )
            : LinearAddress( base, index, offset ), _info( base._info )
        {}

        StateAddress( ProgramInfo *i, Blob b, int offset )
            : LinearAddress( b, offset ), _info( i )
        {}

        StateAddress copy( StateAddress to, int size ) {
            std::copy( dereference(), dereference() + size, to.dereference() );
            return StateAddress( _info, to.b, to.offset + size );
        }
    };

    struct Frame {
        PC pc;
        char memory[0];

        int framesize( ProgramInfo &i ) {
            return i.function( pc ).framesize;
        }

        StateAddress advance( StateAddress a, int ) {
            return StateAddress( a, 0, sizeof( Frame ) + framesize( *a._info ) );
        }
        char *dereference( ProgramInfo &i, ProgramInfo::Value v ) {
            assert_leq( v.offset, framesize( i ) );
            assert_leq( v.offset + v.width, framesize( i ) );
            return memory + v.offset;
        }
        int end() { return 0; }
    };

    struct Globals {
        char memory[0];
        StateAddress advance( StateAddress a, int ) {
            return StateAddress( a, 0, a._info->globalsize );
        }
        int end() { return 0; }
    };

    static int size_jumptable( int segcount ) {
        /* 4-align, extra item for "end of memory" */
        return 2 * (2 + segcount - segcount % 2);
    }

    static int size_bitmap( int bytecount ) {
        /* NB. 4 * (x / 32) is NOT the same as x / 8 */
        return 4 * (bytecount / 32) + ((bytecount % 32) ? 4 : 0);
    }

    static int size_heap( int segcount, int bytecount ) {
        return sizeof( Heap ) +
            bytecount +
            size_jumptable( segcount ) +
            size_bitmap( bytecount );
    }

    struct Heap {
        int segcount;
        char _memory[0];

        int size() {
            return jumptable( segcount ) * 4;
        }

        uint16_t &jumptable( int segment ) {
            return reinterpret_cast< uint16_t * >( _memory )[ segment ];
        }

        uint16_t &jumptable( Pointer p ) {
            assert( owns( p ) );
            return jumptable( p.segment );
        }

        uint16_t &bitmap( Pointer p ) {
            assert( owns( p ) );
            return reinterpret_cast< uint16_t * >(
                _memory + size_jumptable( segcount ) )[ p.segment / 64 ];
        }

        int offset( Pointer p ) {
            assert( owns( p ) );
            return int( jumptable( p ) * 4 ) + p.offset;
        }

        int size( Pointer p ) {
            assert( owns( p ) );
            return 4 * (jumptable( p.segment + 1 ) - jumptable( p ));
        }

        uint16_t mask( Pointer p ) {
            assert_eq( offset( p ) % 4, 0 );
            return 1 << ((offset( p ) % 64) / 4);
        }

        StateAddress advance( StateAddress a, int ) {
            return StateAddress( a, 0, size_heap( segcount, size() ) );
        }
        int end() { return 0; }

        void setPointer( Pointer p, bool ptr ) {
            if ( ptr )
                bitmap( p ) |= mask( p );
            else
                bitmap( p ) &= ~mask( p );
        }

        bool isPointer( Pointer p ) {
            return bitmap( p ) & mask( p );
        }

        bool owns( Pointer p ) {
            return p.valid && p.segment < segcount;
        }

        char *dereference( Pointer p ) {
            assert( owns( p ) );
            return _memory + size_bitmap( size() ) + size_jumptable( segcount ) + offset( p );
        }
    };

    struct Nursery {
        std::vector< int > offsets;
        std::vector< char > memory;
        std::vector< bool > pointer;
        int segshift;

        Pointer malloc( int size ) {
            int segment = offsets.size() - 1;
            int start = offsets[ segment ];
            int end = start + size;
            align( end, 4 );
            offsets.push_back( end );
            memory.resize( end );
            pointer.resize( end / 4 );
            std::fill( memory.begin() + start, memory.end(), 0 );
            std::fill( pointer.begin() + start / 4, pointer.end(), 0 );
            return Pointer( segment + segshift, 0 );
        }

        bool owns( Pointer p ) {
            return p.valid && (p.segment - segshift < offsets.size() - 1);
        }

        int offset( Pointer p ) {
            assert( owns( p ) );
            return offsets[ p.segment - segshift ] + p.offset;
        }

        int size( Pointer p ) {
            assert( owns( p ) );
            return offsets[ p.segment - segshift + 1] - offsets[ p.segment - segshift ];
        }

        bool isPointer( Pointer p ) {
            assert_eq( p.offset % 4, 0 );
            return pointer[ offset( p ) / 4 ];
        }

        void setPointer( Pointer p, bool is ) {
            pointer[ offset( p ) / 4 ] = is;
        }

        char *dereference( Pointer p ) {
            assert( owns( p ) );
            return &memory[ offset( p ) ];
        }

        void reset( int shift ) {
            segshift = shift;
            memory.clear();
            offsets.clear();
            offsets.push_back( 0 );
            pointer.clear();
        }
    };

    Blob _blob, _stack;
    Nursery nursery;
    ProgramInfo &_info;
    Allocator &_alloc;
    int _thread; /* the currently scheduled thread */
    int _thread_count;
    Frame *_frame; /* pointer to the currently active frame */

    template< typename T >
    using Lens = lens::Lens< StateAddress, T >;

    struct Flags {
        uint64_t assert:1;
        uint64_t null_dereference:1;
        uint64_t invalid_dereference:1;
        uint64_t invalid_argument:1;
        uint64_t ap:48;
        uint64_t buchi:12;
    };

    typedef lens::Array< Frame > Stack;
    typedef lens::Array< Stack > Threads;
    typedef lens::Tuple< Flags, Globals, Heap, Threads > State;

    char *dereference( Pointer p ) {
        assert( validate( p ) );
        if ( heap().owns( p ) )
            return heap().dereference( p );
        else
            return nursery.dereference( p );
    }

    bool validate( Pointer p ) {
        return p.valid && ( heap().owns( p ) || nursery.owns( p ) );
    }

    Pointer followPointer( Pointer p ) {
        if ( !validate( p ) )
            return Pointer();

        Pointer next = *reinterpret_cast< Pointer * >( dereference( p ) );
        if ( heap().owns( p ) ) {
            if ( heap().isPointer( p ) )
                return next;
        } else if ( nursery.isPointer( p ) )
            return next;
        return Pointer();
    }

    int pointerSize( Pointer p ) {
        if ( !validate( p ) )
            return 0;

        if ( heap().owns( p ) )
            return heap().size( p );
        else
            return nursery.size( p );
    }

    Lens< State > state() {
        return Lens< State >( StateAddress( &_info, _blob, _alloc._slack ) );
    }

    /*
     * Get a pointer to the value storage corresponding to "v". If "v" is a
     * local variable, look into thread "thread" (default, i.e. -1, for the
     * currently executing one) and in frame "frame" (default, i.e. 0 is the
     * topmost frame, 1 is the frame just below that, etc...).
     */
    char *dereference( ProgramInfo::Value v, int tid = -1, int frame = 0 )
    {
        char *block = _frame->memory;

        if ( tid < 0 )
            tid = _thread;

        if ( !v.global && !v.constant && ( frame || tid != _thread ) )
            block = stack( tid ).get( stack( tid ).get().length() - frame - 1 ).memory;

        if ( v.global )
            block = state().get( Globals() ).memory;

        if ( v.constant )
            block = &_info.constdata[0];

        return block + v.offset;
    }

    void rewind( Blob to, int thread = 0 )
    {
        _blob = to;
        _thread = -1; // special

        _thread_count = threads().get().length();
        nursery.reset( heap().segcount );

        if ( thread >= 0 && thread < threads().get().length() )
            switch_thread( thread );
        // else everything becomes rather unsafe...
    }

    void switch_thread( int thread )
    {
        assert_leq( thread, threads().get().length() - 1 );
        _thread = thread;

        StateAddress stackaddr( &_info, _stack, 0 );
        _blob_stack( _thread ).copy( stackaddr );
        assert( stack().get().length() );
        _frame = &stack().get( stack().get().length() - 1 );
   }

    int new_thread()
    {
        Blob old = _blob;
        _blob = snapshot();
        old.free( _alloc.pool() );
        _thread = _thread_count ++;

        /* Set up an empty thread. */
        stack().get().length() = 0;
        return _thread;
    }

    Lens< Threads > threads() {
        return state().sub( Threads() );
    }

    Lens< Stack > _blob_stack( int i ) {
        assert_leq( 0, i );
        return state().sub( Threads(), i );
    }

    Lens< Stack > stack( int thread = -1 ) {
        if ( thread == _thread || thread < 0 ) {
            assert_leq( 0, _thread );
            return Lens< Stack >( StateAddress( &_info, _stack, 0 ) );
        } else
            return _blob_stack( thread );
    }

    Heap &heap() {
        return state().get( Heap() );
    }

    Frame &frame( int thread = -1, int idx = 0 ) {
         if ( !idx )
             return *_frame;

        auto s = stack( thread );
        return s.get( s.get().length() - idx - 1 );
    }

    Flags &flags() {
        return state().get( Flags() );
    }

    void enter( int function ) {
        int framesize = _info.functions[ function ].framesize;
        int depth = stack().get().length();
        bool masked = depth ? frame().pc.masked : false;
        stack().get().length() ++;

        _frame = &stack().get( depth );
        _frame->pc = PC( function, 0, 0 );
        _frame->pc.masked = masked; /* inherit */
        std::fill( _frame->memory, _frame->memory + framesize, 0 );
    }

    void leave() {
        int fun = frame().pc.function;
        auto &s = stack().get();
        s.length() --;
        if ( stack().get().length() )
            _frame = &stack().get( stack().get().length() - 1 );
        else
            _frame = nullptr;
    }

    struct Canonic {
        MachineState &ms;
        std::map< int, int > segmap;
        int allocated, segcount;
        int stack;
        int boundary, segdone;

        Canonic( MachineState &ms )
            : ms( ms ), allocated( 0 ), segcount( 0 ), stack( 0 ), boundary( 0 ), segdone( 0 )
        {}

        Pointer operator[]( Pointer idx ) {
            if ( !idx.valid )
                return idx;
            if ( !segmap.count( idx.segment ) ) {
                segmap.insert( std::make_pair( int( idx.segment ), segcount ) );
                ++ segcount;
                allocated += ms.pointerSize( idx );
            }
            return Pointer( segmap[ int( idx.segment ) ], idx.offset );
        }
    };

    void trace( Pointer p, Canonic &canonic ) {
        if ( p.valid ) {
            canonic[ p ];
            trace( followPointer( p ), canonic );
        }
    }

    void trace( Frame &f, Canonic &canonic ) {
        auto vals = _info.function( f.pc ).values;
        for ( auto val = vals.begin(); val != vals.end(); ++val ) {
            canonic.stack += val->width;
            if ( val->pointer )
                trace( *reinterpret_cast< Pointer * >( &f.memory[val->offset] ), canonic );
        }
        align( canonic.stack, 4 );
    }

    template< typename F >
    void eachframe( Lens< Stack > s, F f ) {
        int idx = 0;
        auto address = s.sub( 0 ).address();
        while ( idx < s.get().length() ) {
            Frame &fr = address.as< Frame >();
            f( fr );
            address = fr.advance( address, 0 );
            ++ idx;
        }
    }

    int size( int stack, int heapbytes, int heapsegs ) {
        return sizeof( Flags ) +
               sizeof( int ) + /* thread count */
               stack + size_heap( heapsegs, heapbytes ) + _info.globalsize;
    }

    void snapshot( Pointer from, Pointer to, Canonic &canonic, Heap &heap ) {
        if ( !validate( from ) || to.segment < canonic.segdone )
            return; /* invalid or done */
        assert_eq( to.segment, canonic.segdone );
        canonic.segdone ++;
        int size = pointerSize( from );
        heap.jumptable( to ) = canonic.boundary / 4;
        canonic.boundary += size;

        /* Work in 4 byte steps, since pointers are 4 bytes and 4-byte aligned. */
        from.offset = to.offset = 0;
        for ( ; from.offset < size; from.offset += 4, to.offset += 4 ) {
            Pointer p = followPointer( from ), q = canonic[ p ];
            heap.setPointer( to, !p.null() );
            if ( p.null() ) /* not a pointer, make a straight copy */
                std::copy( dereference( from ), dereference( from ) + 4,
                           heap.dereference( to ) );
            else { /* recurse */
                *reinterpret_cast< Pointer * >( heap.dereference( to ) ) = q;
                snapshot( p, q, canonic, heap );
            }
        }
    }

    void snapshot( Frame &f, Canonic &canonic, Heap &heap, StateAddress &address ) {
        auto vals = _info.function( f.pc ).values;
        address.as< PC >() = f.pc;
        address.advance( sizeof( PC ) );
        for ( auto val = vals.begin(); val != vals.end(); ++val ) {
            char *from_addr = f.dereference( _info, *val );
            if ( val->pointer ) {
                Pointer from = *reinterpret_cast< Pointer * >( from_addr );
                Pointer to = canonic[ from ];
                address.as< Pointer >() = to;
                snapshot( from, to, canonic, heap );
            } else
                std::copy( from_addr, from_addr + val->width, address.dereference() );
            address.advance( val->width );
        }
        align( address.offset, 4 );
    }

    Blob snapshot() {
        Canonic canonic( *this );
        int live_threads = _thread_count;

        for ( int tid = 0; tid < _thread_count; ++tid ) {
            if ( !stack( tid ).get().length() ) { /* leave out dead threads */
                -- live_threads;
                continue;
            }

            canonic.stack += sizeof( Stack );
            eachframe( stack( tid ), [&]( Frame &fr ) {
                    canonic.stack += sizeof( Frame );
                    trace( fr, canonic );
                } );
        }

        Blob b = _alloc.new_blob(
            size( canonic.stack, canonic.allocated, canonic.segcount ) );

        StateAddress address( &_info, b, _alloc._slack );
        address = state().sub( Flags() ).copy( address );
        assert_eq( address.offset, _alloc._slack + sizeof( Flags ) );
        address = state().sub( Globals() ).copy( address );

        /* skip the heap */
        Heap *_heap = &address.as< Heap >();
        _heap->segcount = canonic.segcount;
        /* heap needs to know its size in order to correctly dereference! */
        _heap->jumptable( canonic.segcount ) = canonic.allocated / 4;
        address.advance( size_heap( canonic.segcount, canonic.allocated ) );
        assert_eq( size_heap( canonic.segcount, canonic.allocated ) % 4, 0 );

        address.as< int >() = live_threads;
        address.advance( sizeof( int ) ); // ick. length of the threads array

        for ( int tid = 0; tid < _thread_count; ++tid ) {
            if ( !stack( tid ).get().length() )
                continue;

            address.as< int >() = stack( tid ).get().length();
            address.advance( sizeof( int ) );
            eachframe( stack( tid ), [&]( Frame &fr ) {
                    snapshot( fr, canonic, *_heap, address );
                });
        }

        assert_eq( canonic.segdone, canonic.segcount );
        assert_eq( canonic.boundary, canonic.allocated );
        assert_eq( address.offset, b.size() );
        assert_eq( b.size() % 4, 0 );

        return b;
    }

    MachineState( ProgramInfo &i, Allocator &alloc )
        : _stack( 4096 ), _info( i ), _alloc( alloc )
    {
        _thread_count = 0;
        _frame = nullptr;
        nursery.reset( 0 ); /* nothing in the heap */
    }

    void dump( std::ostream & );
};

}
}

#endif

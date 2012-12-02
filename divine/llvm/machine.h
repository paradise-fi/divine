// -*- C++ -*- (c) 2011, 2012 Petr Rockai

#include <divine/llvm/program.h>
#include <divine/toolkit/lens.h>
#include <divine/graph/allocator.h>

#ifndef DIVINE_LLVM_MACHINE_H
#define DIVINE_LLVM_MACHINE_H

namespace divine {
namespace llvm {

struct Canonic;

struct Problem {
    enum What { Assert, InvalidDereference, InvalidArgument };
    PC where;
    uint8_t what;
    uint8_t tid;
    uint16_t _padding;
};

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
        uint8_t memory[0];

        void clear( ProgramInfo &i ) {
            std::fill( memory, memory + framesize( i ), 0 );
        }

        int framesize( ProgramInfo &i ) {
            int size = datasize( i ) + size_bitmap( datasize( i ), 1 );
            align( size, 4 );
            return size;
        }

        int datasize( ProgramInfo &i ) {
            return i.function( pc ).datasize;
        }

        uint8_t &bitmap( ProgramInfo &i, ProgramInfo::Value v ) {
            return *(memory + datasize( i ) + v.offset / 32);
        }

        uint8_t mask( ProgramInfo::Value v ) {
            return 1 << ((v.offset % 32) / 4);
        }

        bool isPointer( ProgramInfo &i, ProgramInfo::Value v ) {
            return bitmap( i, v ) & mask( v );
        }

        void setPointer( ProgramInfo &i, ProgramInfo::Value v, bool ptr ) {
            if ( ptr )
                bitmap( i, v ) |= mask( v );
            else
                bitmap( i, v ) &= ~mask( v );
        }

        StateAddress advance( StateAddress a, int ) {
            return StateAddress( a, 0, sizeof( Frame ) + framesize( *a._info ) );
        }
        int end() { return 0; }

        template< typename T = char >
        T *dereference( ProgramInfo &i, ProgramInfo::Value v ) {
            assert_leq( v.offset, datasize( i ) );
            assert_leq( v.offset + v.width, datasize( i ) );
            return reinterpret_cast< T * >( memory + v.offset );
        }
    };

    struct Globals {
        uint8_t memory[0];

        StateAddress advance( StateAddress a, int ) {
            return StateAddress( a, 0, size( *a._info ) );
        }
        int end() { return 0; }

        static int size( ProgramInfo &i ) {
            return i.globalsize + size_bitmap( i.globalsize );
        }

        uint8_t &bitmap( ProgramInfo &i, Pointer p ) {
            return *(memory + i.globalsize + i.globalPointerOffset( p ) / 32);
        }

        uint8_t mask( ProgramInfo &i, Pointer v ) {
            return 1 << ((i.globalPointerOffset( v ) % 32) / 4);
        }

        bool isPointer( ProgramInfo &i, Pointer p ) {
            return bitmap( i, p ) & mask( i, p );
        }

        void setPointer( ProgramInfo &i, Pointer p, bool ptr ) {
            if ( ptr )
                bitmap( i, p ) |= mask( i, p );
            else
                bitmap( i, p ) &= ~mask( i, p );
        }

        bool owns( ProgramInfo &i, Pointer p ) {
            return !p.heap && p.segment >= 1 && p.segment < i.globals.size() &&
                   !i.globals[p.segment].constant;
        }

        template< typename T = char >
        T *dereference( ProgramInfo &i, Pointer p ) {
            assert( owns( i, p ) );
            return reinterpret_cast< T * >(
                memory + i.globalPointerOffset( p ) );
        }
    };

    static int size_jumptable( int segcount ) {
        /* 4-align, extra item for "end of memory" */
        return 2 * (2 + segcount - segcount % 2);
    }

    static int size_bitmap( int bytecount, int align = 4 ) {
        int size = bytecount / 32 + ((bytecount % 32) ? 1 : 0); /* bytes */
        llvm::align( size, align );
        return size;
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
                _memory + size_jumptable( segcount ) )[ offset( p ) / 64 ];
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
            return p.heap && p.segment < segcount;
        }

        template< typename T = char >
        T *dereference( Pointer p ) {
            assert( owns( p ) );
            return reinterpret_cast< T * >(
                _memory + size_bitmap( size() ) + size_jumptable( segcount ) + offset( p ) );
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
            return Pointer( true, segment + segshift, 0 );
        }

        bool owns( Pointer p ) {
            return p.heap && p.segment >= segshift && p.segment - segshift < offsets.size() - 1;
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
            assert_leq( offset( p ), offsets[ offsets.size() - 1 ] );
            assert_leq( offsets[ p.segment - segshift ] + size( p ), offsets[ offsets.size() - 1 ] );
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
    std::set< int > freed;
    std::vector< Problem > problems;

    ProgramInfo &_info;
    Allocator &_alloc;
    int _thread; /* the currently scheduled thread */
    int _thread_count;
    Frame *_frame; /* pointer to the currently active frame */
    bool _blob_private;

    template< typename T >
    using Lens = lens::Lens< StateAddress, T >;

    struct Flags {
        uint64_t problemcount:7;
        uint64_t problem:1;
        uint64_t ap:44;
        uint64_t buchi:12;
        Problem problems[0];

        void clear() {
            problem = 0;
        }

        bool bad() {
            return problem;
        }

        StateAddress advance( StateAddress a, int ) {
            return StateAddress( a, 0, sizeof( Flags ) + problemcount * sizeof( Problem ) );
        }
        int end() { return 0; }
    };

    typedef lens::Array< Frame > Stack;
    typedef lens::Array< Stack > Threads;
    typedef lens::Tuple< Flags, Globals, Heap, Threads > State;

    bool globalPointer( Pointer p ) {
        return global().owns( _info, p );
    }

    bool validate( Pointer p ) {
        return !p.null() &&
            ( globalPointer( p ) ||
              ( !freed.count( p.segment ) &&
                ( heap().owns( p ) || nursery.owns( p ) ) ) );
    }

    Pointer malloc( int size ) { return nursery.malloc( size ); }
    void free( Pointer p ) {
        if ( p.null() )
            return; /* nothing to do */
        assert( p.heap );
        assert( validate( p ) );
        freed.insert( p.segment );
    }

    Lens< State > state() {
        return Lens< State >( StateAddress( &_info, _blob, _alloc._slack ) );
    }

    bool isPointer( Pointer p ) {
        if ( nursery.owns( p ) )
            return nursery.isPointer( p );
        if ( heap().owns( p ) )
            return heap().isPointer( p );
        if ( globalPointer( p ) )
            return global().isPointer( _info, p );
        return false;
    }

    void setPointer( Pointer p, bool is ) {
        if ( nursery.owns( p ) )
            nursery.setPointer( p, is );
        if ( heap().owns( p ) )
            heap().setPointer( p, is );
        if ( globalPointer( p ) )
            global().setPointer( _info, p, is );
    }

    char *dereference( Pointer p ) {
        if( !validate( p ) )
            return nullptr;
        else if ( globalPointer( p ) )
            return global().dereference( _info, p );
        else if ( heap().owns( p ) )
            return heap().dereference( p );
        else
            return nursery.dereference( p );
    }

    Frame &frame( ValueRef v ) {
        assert( !v.v.global );
        assert( !v.v.constant );
        Frame *f = _frame;
        if ( v.tid != _thread || v.frame )
            f = &stack( v.tid ).get( stack( v.tid ).get().length() - v.frame - 1 );
        return *f;
    }

    bool isPointer( ValueRef v ) {
        if ( v.tid < 0 )
            v.tid = _thread;
        if ( v.v.constant )
            return false; /* can't point to heap by definition */
        assert( !v.v.global );
        return frame( v ).isPointer( _info, v.v );
    }

    void setPointer( ValueRef v, bool is ) {
        if ( v.tid < 0 )
            v.tid = _thread;
        if ( v.v.constant )
            return;
        assert( !v.v.global );
        frame( v ).setPointer( _info, v.v, is );
    }

    /*
     * Get a pointer to the value storage corresponding to "v". If "v" is a
     * local variable, look into thread "thread" (default, i.e. -1, for the
     * currently executing one) and in frame "frame" (default, i.e. 0 is the
     * topmost frame, 1 is the frame just below that, etc...).
     */
    char *dereference( ValueRef v )
    {
        if ( v.tid < 0 )
            v.tid = _thread;

        if ( !v.v.global && !v.v.constant )
            return frame( v ).dereference( _info, v.v );

        if ( v.v.constant )
            return &_info.constdata[v.v.offset];

        if ( v.v.global )
            return reinterpret_cast< char * >( global().memory + v.v.offset );

        assert_unreachable( "Impossible Value." );
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

    Globals &global() {
        return state().get( Globals() );
    }

    Frame &frame( int thread = -1, int idx = 0 ) {
        if ( ( thread == _thread || thread < 0 ) && !idx )
             return *_frame;

        auto s = stack( thread );
        return s.get( s.get().length() - idx - 1 );
    }

    Flags &flags() {
        return state().get( Flags() );
    }

    void enter( int function ) {
        int depth = stack().get().length();
        bool masked = depth ? frame().pc.masked : false;
        stack().get().length() ++;

        _frame = &stack().get( depth );
        _frame->pc = PC( function, 0, 0 );
        _frame->pc.masked = masked; /* inherit */
        _frame->clear( _info );
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

    void resnap();
    void switch_thread( int thread );
    int new_thread();

    int pointerSize( Pointer p );
    Pointer &followPointer( Pointer p );

    void trace( Pointer p, Canonic &canonic );
    void trace( Frame &f, Canonic &canonic );
    void snapshot( Pointer &edit, Pointer original, Canonic &canonic, Heap &heap );
    void snapshot( Frame &f, Canonic &canonic, Heap &heap, StateAddress &address );
    Blob snapshot();
    void rewind( Blob, int thread = 0 );
    void problem( Problem::What );

    int size( int stack, int heapbytes, int heapsegs, int problems ) {
        return sizeof( Flags ) +
               sizeof( Problem ) * problems +
               sizeof( int ) + /* thread count */
               stack + size_heap( heapsegs, heapbytes ) + Globals::size( _info );
    }

    MachineState( ProgramInfo &i, Allocator &alloc )
        : _stack( 4096 ), _info( i ), _alloc( alloc ), _blob_private( false )
    {
        _thread_count = 0;
        _frame = nullptr;
        nursery.reset( 0 ); /* nothing in the heap */
    }

    void dump( std::ostream & );
    void dump();
};

}
}

#endif

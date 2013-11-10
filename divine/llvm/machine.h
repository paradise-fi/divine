// -*- C++ -*- (c) 2011, 2012 Petr Rockai

#include <divine/llvm/program.h>
#include <divine/toolkit/lens.h>
#include <divine/graph/graph.h>

#include <wibble/param.h>

#ifndef DIVINE_LLVM_MACHINE_H
#define DIVINE_LLVM_MACHINE_H

namespace divine {
namespace llvm {

struct Canonic;

struct Problem {
    enum What {
        NoProblem = 0,
        Other = 1,
        Assert,
        InvalidDereference,
        InvalidArgument,
        OutOfBounds,
        DivisionByZero,
        UnreachableExecuted,
        MemoryLeak,
        NotImplemented,
        Uninitialised
    };
    PC where;
    uint8_t what;
    uint8_t tid;
    uint16_t _padding;
    Pointer pointer;
    Problem() : what( NoProblem ), tid( -1 ), _padding( 0 ) {}
};

template < typename From, typename To, typename FromC, typename ToC >
Problem::What memcopy( From f, To t, int bytes, FromC &fromc, ToC &toc )
{
    if ( !bytes )
        return Problem::NoProblem; /* this is apparently always OK */

    char *from = fromc.dereference( f ),
           *to = toc.dereference( t );
    int f_end = f.offset + bytes;

    if ( !from || !to )
        return Problem::InvalidDereference;

    if ( !fromc.inBounds( f, bytes - 1 ) ||
         !toc.inBounds( t, bytes - 1 ) )
        return Problem::OutOfBounds;

    memmove( to, from, bytes );

    int unalignment = t.offset % 4;

    std::vector< std::pair< To, MemoryFlag > > setflags;

    /* check whether we are writing over part of a (former) pointer */
    if ( unalignment ) {
        t.offset -= unalignment;
        if ( toc.memoryflag( t ).get() == MemoryFlag::HeapPointer )
            setflags.emplace_back( t, MemoryFlag::Uninitialised );
        t.offset += unalignment;
    }

    while ( f.offset < f_end ) {
        setflags.emplace_back( t, fromc.memoryflag( f ).get() );
        f.offset ++;
        t.offset ++;
    }

    for ( auto s : setflags )
        toc.memoryflag( s.first ).set( s.second );

    return Problem::NoProblem;
}

struct MachineState
{
    struct StateAddress : lens::LinearAddress
    {
        ProgramInfo *_info;

        StateAddress() : _info( nullptr ) {}

        StateAddress( StateAddress base, int index, int offset )
            : LinearAddress( base, index, offset ), _info( base._info )
        {}

        StateAddress( Pool* pool, ProgramInfo *i, Blob b, int offset )
            : LinearAddress( pool, b, offset ), _info( i )
        {}

        StateAddress copy( StateAddress to, int size ) {
            std::copy( dereference(), dereference() + size, to.dereference() );
            return StateAddress( pool, _info, to.b, to.offset + size );
        }
    };

    template< typename F >
    struct WithMemory {
        template< typename T >
        struct BaseSize : T { int x; };

        uint8_t *memory() {
            return reinterpret_cast< uint8_t * >( this ) + sizeof( BaseSize< F > ) - sizeof( int );
        }
    };

    struct Frame : WithMemory< Frame > {
        PC pc;

        void clear( ProgramInfo &i ) {
            std::fill( memory(), memory() + framesize( i ), 0 );
        }

        static int framesize( ProgramInfo &i, int dsize ) {
            return align( dsize + size_memoryflags( dsize ), 4 );
        }

        int framesize( ProgramInfo &i ) {
            return framesize( i, datasize( i ) );
        }

        int datasize( ProgramInfo &i ) {
            return i.function( pc ).datasize;
        }

        MemoryBits memoryflag( ProgramInfo &i, ValueRef v ) {
            return MemoryBits( memory() + datasize( i ),
                               v.offset + v.v.offset );
        }

        StateAddress advance( StateAddress a, int ) {
            return StateAddress( a, 0, sizeof( Frame ) + framesize( *a._info ) );
        }

        int end() { return 0; }

        template< typename T = char >
        T *dereference( ProgramInfo &i, ValueRef v ) {
            assert_leq( int( v.v.offset + v.offset ), datasize( i ) );
            wibble::param::discard( i );
            return reinterpret_cast< T * >( memory() + v.offset + v.v.offset );
        }
    };

    struct Globals : WithMemory< Globals > {
        StateAddress advance( StateAddress a, int ) {
            return StateAddress( a, 0, size( *a._info ) );
        }
        int end() { return 0; }

        static int size( ProgramInfo &i ) {
            return i.globalsize + size_memoryflags( i.globalsize );
        }

        MemoryBits memoryflag( ProgramInfo &i, Pointer p ) {
            assert( owns( i, p ) );
            return MemoryBits( memory() + i.globalsize, i.globalPointerOffset( p ) );
        }

        MemoryBits memoryflag( ProgramInfo &i ) {
            return MemoryBits( memory() + i.globalsize, 0 );
        }

        bool owns( ProgramInfo &i, Pointer p ) {
            return !p.heap && p.segment >= 1 && p.segment < i.globals.size() &&
                   !i.globals[p.segment].constant;
        }

        template< typename T = char >
        T *dereference( ProgramInfo &i, Pointer p ) {
            assert( owns( i, p ) );
            if ( !i.globalPointerInBounds( p ) )
                return nullptr;
            return reinterpret_cast< T * >(
                memory() + i.globalPointerOffset( p ) );
        }
    };

    static int size_jumptable( int segcount ) {
        /* 4-align, extra item for "end of memory" */
        return 2 * (2 + segcount - segcount % 2);
    }

    static int size_memoryflags( int bytecount ) {
        const int bitcount = bytecount * MemoryBits::bitwidth;
        return divine::align( bitcount / 8 + ((bitcount % 8) ? 1 : 0), 4 );
    }

    static int size_heap( int segcount, int bytecount ) {
        return sizeof( Heap ) +
            bytecount +
            size_jumptable( segcount ) +
            size_memoryflags( bytecount );
    }

    struct Heap : WithMemory< Heap > {
        int segcount;

        int size() {
            return jumptable( segcount ) * 4;
        }

        uint16_t &jumptable( int segment ) {
            return reinterpret_cast< uint16_t * >( memory() )[ segment ];
        }

        uint16_t &jumptable( Pointer p ) {
            assert( owns( p ) );
            return jumptable( p.segment );
        }

        MemoryBits memoryflag( Pointer p ) {
            assert( owns( p ) );
            return MemoryBits( memory() + size_jumptable( segcount ),
                               offset( p ) );
        }

        bool inBounds( Pointer p, int off ) {
            p.offset += off; return dereference( p );
        }

        int offset( Pointer p ) {
            assert( owns( p ) );
            return int( jumptable( p ) * 4 ) + p.offset;
        }

        int size( Pointer p ) {
            assert( owns( p ) );
            return 4 * (jumptable( p.segment + 1 ) - jumptable( p ));
        }

        StateAddress advance( StateAddress a, int ) {
            return StateAddress( a, 0, size_heap( segcount, size() ) );
        }
        int end() { return 0; }

        bool owns( Pointer p ) {
            return p.heap && p.segment < segcount;
        }

        template< typename T = char >
        T *dereference( Pointer p ) {
            assert( owns( p ) );
            if ( p.offset >= size( p ) )
                return nullptr;
            return reinterpret_cast< T * >(
                memory() + size_memoryflags( size() ) + size_jumptable( segcount ) + offset( p ) );
        }
    };

    struct Nursery {
        std::vector< int > offsets;
        std::vector< char > memory;
        std::vector< uint8_t > flags;
        int segshift;

        Pointer malloc( int size ) {
            int segment = offsets.size() - 1;
            int start = offsets[ segment ];
            int end = align( start + size, 4 );
            offsets.push_back( end );
            memory.resize( end, 0 );
            flags.resize( size_memoryflags( end ), 0 );
            return Pointer( true, segment + segshift, 0 );
        }

        bool owns( Pointer p ) {
            return p.heap && p.segment >= segshift &&
                   p.segment - segshift < int( offsets.size() ) - 1;
        }

        int offset( Pointer p ) {
            assert( owns( p ) );
            return offsets[ p.segment - segshift ] + p.offset;
        }

        int size( Pointer p ) {
            assert( owns( p ) );
            return offsets[ p.segment - segshift + 1] - offsets[ p.segment - segshift ];
        }

        MemoryBits memoryflag( Pointer p ) {
            return MemoryBits( &flags.front(), offset( p ) );
        }

        char *dereference( Pointer p ) {
            assert( owns( p ) );
            if ( offset( p ) >= offsets[ offsets.size() - 1 ] )
                return nullptr;
            assert_leq( offsets[ p.segment - segshift ] + size( p ), offsets[ offsets.size() - 1 ] );
            return &memory[ offset( p ) ];
        }

        void reset( int shift ) {
            segshift = shift;
            memory.clear();
            offsets.clear();
            offsets.push_back( 0 );
            flags.clear();
        }
    };

    Blob _blob;
    /* those stacks experienced an entry and fit in _blob no more */
    std::vector< std::pair< bool, Blob > > _stack;
    Nursery nursery;
    std::set< int > freed;
    std::vector< Problem > problems;

    ProgramInfo &_info;
    graph::Allocator &_alloc; /* XXX */
    int _thread; /* the currently scheduled thread */
    int _thread_count;
    Frame *_frame; /* pointer to the currently active frame */

    template< typename T >
    using Lens = lens::Lens< StateAddress, T >;

    struct Flags : WithMemory< Flags >
    {
        uint64_t problemcount:7;
        uint64_t problem:1;
        uint64_t ap:44;
        uint64_t buchi:12;
        Problem &problems( int i ) {
            return *( reinterpret_cast< Problem * >( memory() ) + i );
        }

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

    bool constantPointer( Pointer p ) {
        return  !p.heap && p.segment >= 1 && p.segment < _info.globals.size() &&
                _info.globals[ p.segment ].constant;
    }

    bool validate( Pointer p ) {
        return !p.null() &&
            ( globalPointer( p ) || constantPointer( p ) ||
              ( !freed.count( p.segment ) &&
                ( heap().owns( p ) || nursery.owns( p ) ) ) );
    }

    Pointer malloc( int size ) { return nursery.malloc( size ); }
    bool free( Pointer p ) {
        if ( p.null() )
            return true; /* nothing to do */
        if( !validate( p ) || !p.heap )
            return false;
        freed.insert( p.segment );
        return true;
    }

    bool isPrivate( int tid, Pointer p );
    bool isPrivate( Pointer p, Frame &, Canonic & );
    bool isPrivate( Pointer p, Pointer, Canonic & );

    Lens< State > state() {
        return Lens< State >( StateAddress( &_alloc.pool(), &_info, _blob, _alloc._slack ) );
    }

    MemoryBits memoryflag( Pointer p, int offset = 0 ) {
        p.offset += offset; /* beware of dragons! */
        if ( nursery.owns( p ) )
            return nursery.memoryflag( p );
        if ( heap().owns( p ) )
            return heap().memoryflag( p );
        if ( globalPointer( p ) )
            return global().memoryflag( _info, p );
        assert_unreachable( "invalid pointer passed to memoryflags" );
    }

    MemoryFlag _const_flag;

    MemoryBits memoryflag( ValueRef v ) {
        if ( v.tid < 0 )
            v.tid = _thread;
        if ( v.v.constant ) {
            assert( _const_flag == MemoryFlag::Data );
            return MemoryBits( reinterpret_cast< uint8_t * >( &_const_flag ), 0 );
        }
        assert( !v.v.global );
        return frame( v ).memoryflag( _info, v );
    }

    char *dereference( Pointer p ) {
        if( !validate( p ) )
            return nullptr;
        else if ( globalPointer( p ) )
            return global().dereference( _info, p );
        else if ( constantPointer( p ) ) {
            if ( _info.globalPointerInBounds( p ) )
                return &_info.constdata[ _info.globalPointerOffset( p ) ];
            else
                return nullptr;
        } else if ( heap().owns( p ) )
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
            return frame( v ).dereference( _info, v );

        if ( v.v.constant )
            return &_info.constdata[ v.v.offset + v.offset ];

        if ( v.v.global )
            return reinterpret_cast< char * >( global().memory() + v.v.offset + v.offset );

        assert_unreachable( "Impossible Value." );
    }

    bool inBounds( ValueRef v, int byteoff )
    {
        return v.offset + byteoff < v.v.width;
    }

    bool inBounds( Pointer p, int byteoff )
    {
        p.offset += byteoff;
        return dereference( p );
    }

    Lens< Threads > threads() {
        return state().sub( Threads() );
    }

    Lens< Stack > _blob_stack( int i ) {
        assert_leq( 0, i );
        assert_leq( i, threads().get().length() - 1 );
        return state().sub( Threads(), i );
    }

    void detach_stack( int thread, int needbytes ) {
        if ( int( _stack.size() ) <= thread )
            _stack.resize( thread + 1, std::make_pair( false, Blob() ) );

        StateAddress current, next;
        auto &ss = _stack[thread];
        auto &pool = _alloc.pool();

        if ( !pool.valid( ss.second ) )
            ss.second = pool.allocate( std::max( 65536, needbytes ) );

        current = next = StateAddress( &pool, &_info, ss.second, 0 );

        if ( !ss.first ) {
            if ( thread < threads().get().length() )
                current = state().address( Threads(), thread );
            pool.get< int >( ss.second ) = 0; /* clear any pre-existing state */
        }


        Lens< Stack > from( current );
        int neednow = lens::LinearSize< Stack >::get( current );

        if ( neednow + needbytes > pool.size( next.b ) ) {
            next = StateAddress( &pool, &_info,
                                 ss.second = pool.allocate( neednow + needbytes ), 0 );
        }

        if ( current.b != ss.second ) {
            from.copy( next );
            if ( ss.first )
                pool.free( current.b );
        }

        ss.first = true; /* activate */
    }

    Lens< Stack > stack( int thread = -1 )
    {
        if ( thread < 0 )
            thread = _thread;

        if ( thread < int( _stack.size() ) && _stack[thread].first )
            return Lens< Stack >( StateAddress( &_alloc.pool(), &_info, _stack[thread].second, 0 ) );
        else
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
        PC target( function, 0 );
        detach_stack( _thread,
                      sizeof( Frame ) + Frame::framesize( _info, _info.function( target ).datasize ) );
        int depth = stack().get().length();
        bool masked = depth ? frame().pc.masked : false;
        stack().get().length() ++;

        _frame = &stack().get( depth );
        _frame->pc = PC( function, 0 );
        _frame->pc.masked = masked; /* inherit */
        _frame->clear( _info );
    }

    void leave() {
        /* TODO this isn't very efficient and shouldn't be necessary, but we
         * currently can't keep partially empty stacks in the blob, since their
         * length is used to compute the offset of the following stack. */
        detach_stack( _thread, 0 );

        auto &fun = _info.function( frame().pc );

        /* Clear any alloca'd heap objects created by this frame. */
        for ( auto v : fun.values )
            if ( v.type == ProgramInfo::Value::Alloca )
                free( followPointer( v ) );

        if ( fun.vararg )
            free( followPointer( fun.values[ fun.argcount ] ) );

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

    void switch_thread( int thread );
    int new_thread();

    int pointerSize( Pointer p );
    template< typename V >
    Pointer &followPointer( V p ) {
        return *reinterpret_cast< Pointer * >( dereference( p ) );
    }


    void trace( Pointer p, Canonic &canonic );
    void trace( Frame &f, Canonic &canonic );
    void snapshot( Pointer &edit, Pointer original, Canonic &canonic, Heap &heap );
    void snapshot( Frame &f, Canonic &canonic, Heap &heap, StateAddress &address );
    Blob snapshot();
    void rewind( Blob, int thread = 0 );
    void problem( Problem::What, Pointer p = Pointer() );

    int size( int stack, int heapbytes, int heapsegs, int problems ) {
        return sizeof( Flags ) +
               sizeof( Problem ) * problems +
               sizeof( int ) + /* thread count */
               stack + size_heap( heapsegs, heapbytes ) + Globals::size( _info );
    }

    MachineState( ProgramInfo &i, graph::Allocator &alloc )
        : _info( i ), _alloc( alloc ), _const_flag( MemoryFlag::Data )
    {
        _thread_count = 0;
        _frame = nullptr;
        nursery.reset( 0 ); /* nothing in the heap */
    }

    void dump( std::ostream & );
    void dump();
};

struct FrameContext {
    ProgramInfo &info;
    MachineState::Frame &frame;

    template< typename X > MemoryBits memoryflag( X x ) { return frame.memoryflag( info, x ); }
    template< typename X > char *dereference( X x ) { return frame.dereference( info, x ); }
    template< typename X > bool inBounds( X x, int off ) {
        x.offset += off;
        return dereference( x );
    }

    FrameContext( ProgramInfo &i, MachineState::Frame &f )
        : info( i ), frame( f )
    {}
};


}
}

#endif

// -*- C++ -*- (c) 2011-2014 Petr Ročkai <me@mornfall.net>

#include <brick-types.h>
#include <brick-bitlevel.h>
#include <brick-assert.h>

#include <divine/llvm/program.h>
#include <divine/toolkit/lens.h>

#include <llvm/IR/Constants.h>
#include <llvm/IR/Metadata.h>

#ifndef DIVINE_LLVM_MACHINE_H
#define DIVINE_LLVM_MACHINE_H

namespace divine {
namespace llvm {

using brick::types::Maybe;

struct Problem {
    enum What {
        #define PROBLEM(x) x,
        #include <divine/llvm/problem.def>
        #undef PROBLEM
    };
    PC where;
    uint8_t what;
    uint8_t tid;
    uint16_t _padding;
    Pointer pointer;
    Problem() : what( NoProblem ), tid( -1 ), _padding( 0 ) {}
};

template < typename From, typename To, typename FromC, typename ToC >
void memcopy_fastpath( From f, To t, int bytes, FromC &fromc, ToC &toc, char *from, char *to )
{
    memcpy( to, from, bytes );
    auto fmf = fromc.memoryflag( f ), tmf = toc.memoryflag( t );
    if (!tmf.valid() )
        return;

    if ( fmf.valid() )
        bitcopy( fmf, tmf, bytes * MemoryBits::bitwidth );
    else if ( tmf.valid() )
        for ( int i = 0; i < bytes; ++i )
            (tmf++).set( MemoryFlag::Data );
}

template < typename From, typename To, typename FromC, typename ToC >
void memcopy_unsafe( From f, To t, int bytes, FromC &fromc, ToC &toc, char *from, char *to )
{
    int distance = to - from;
    int unalignment = t.offset % 4;
    Maybe< To > clobbered = Maybe< To >::Nothing();
    int f_end = f.offset + bytes;

    if ( unalignment ) {
        t.offset -= unalignment;
        auto tmf = toc.memoryflag( t );
        if ( tmf.valid() && tmf.get() == MemoryFlag::HeapPointer )
            clobbered = Maybe< To >::Just( t );
        t.offset += unalignment;
    }

    if ( abs( distance ) >= bytes ) {
        memcopy_fastpath( f, t, bytes, fromc, toc, from, to );
        if ( clobbered.isJust() )
            toc.memoryflag( clobbered.value() ).set( MemoryFlag::Uninitialised );
    } else {
        memmove( to, from, bytes );

        if ( !toc.memoryflag( t ).valid() )
            return;

        std::vector< std::pair< To, MemoryFlag > > setflags;
        if ( clobbered.isJust() )
            setflags.emplace_back( clobbered.value(), MemoryFlag::Uninitialised );

        while ( f.offset < f_end ) {
            auto fmf = fromc.memoryflag( f );
            setflags.emplace_back( t, fmf.valid() ? fmf.get() : MemoryFlag::Data );
            f.offset ++;
            t.offset ++;
        }

        for ( auto s : setflags )
            toc.memoryflag( s.first ).set( s.second );
    }

}

template < typename From, typename To, typename FromC, typename ToC >
void memcopy_fastpath( From f, To t, int bytes, FromC &fromc, ToC &toc )
{
    char *from = fromc.dereference( f ),
           *to = toc.dereference( t );
    memcopy_fastpath( f, t, bytes, fromc, toc, from, to );
}

template < typename From, typename To, typename FromC, typename ToC >
Problem::What memcopy( From f, To t, int bytes, FromC &fromc, ToC &toc )
{
    if ( !bytes )
        return Problem::NoProblem; /* this is apparently always OK */

    char *from = fromc.dereference( f ),
           *to = toc.dereference( t );

    if ( !from || !to )
        return Problem::InvalidDereference;

    if ( !fromc.inBounds( f, bytes - 1 ) ||
         !toc.inBounds( t, bytes - 1 ) )
        return Problem::OutOfBounds;

    memcopy_unsafe( f, t, bytes, fromc, toc, from, to );

    return Problem::NoProblem;
}

template< typename _HeapMeta >
struct MachineState;

namespace machine {

template< typename HeapMeta >
struct Canonic
{
    MachineState< HeapMeta > &ms;
    std::vector< int > segmap;
    int allocated, segcount;
    int stack;
    int boundary, segdone;

    Canonic( MachineState< HeapMeta > &ms )
        : ms( ms ), allocated( 0 ), segcount( 0 ), stack( 0 ), boundary( 0 ), segdone( 0 )
    {
        segmap.resize( ms.nursery.offsets.size() - 1 + ms.nursery.segshift, -1 );
    }

    Pointer operator[]( Pointer idx ) {
        if ( !idx.heap || !ms.validate( idx ) )
            return idx;
        if ( segmap[ idx.segment ] < 0 ) {
            segmap[ idx.segment ] = segcount ++;
            allocated += ms.pointerSize( idx );
        }
        return Pointer( idx.heap, segmap[ idx.segment ], idx.offset );
    }

    bool seen( Pointer p ) {
        return segmap[ p.segment ] >= 0;
    }
};

inline int size_jumptable( int segcount );
inline int size_memoryflags( int bytecount );
inline int size_heap( int segcount, int bytecount );

template< typename F >
struct WithMemory {
    uint8_t *memory() {
        return reinterpret_cast< uint8_t * >( this ) +
            ( std::is_empty< F >::value ? 0 : sizeof( F ) );
    }
};

struct StateAddress : lens::LinearAddress
{
    ProgramInfo *_info;

    StateAddress() : _info( nullptr ) {}

    StateAddress( StateAddress base, int index, int offset )
        : LinearAddress( base, index, offset ), _info( base._info )
    {}

    StateAddress( ProgramInfo *i, char *b, int offset )
        : LinearAddress( b, offset ), _info( i )
    {}

    StateAddress copy( StateAddress to, int size ) {
        std::copy( dereference(), dereference() + size, to.dereference() );
        return StateAddress( _info, to.b, to.offset + size );
    }
};

struct Frame : WithMemory< Frame > {
    PC pc;

    void clear( ProgramInfo &i ) {
        std::fill( memory(), memory() + framesize( i ), 0 );
    }

    static int framesize( ProgramInfo &, int dsize ) {
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
        ASSERT_LEQ( int( v.v.offset + v.offset ), datasize( i ) );
        brick::_assert::unused( i );
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
        ASSERT( owns( i, p ) );
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
        ASSERT( owns( i, p ) );
        if ( !i.globalPointerInBounds( p ) )
            return nullptr;
        return reinterpret_cast< T * >(
            memory() + i.globalPointerOffset( p ) );
    }
};

struct Heap : WithMemory< Heap > {
    int segcount;

    int size() {
        return jumptable( segcount ) * 4;
    }

    uint16_t &jumptable( int segment ) {
        return reinterpret_cast< uint16_t * >( memory() )[ segment ];
    }

    uint16_t &jumptable( Pointer p ) {
        ASSERT( owns( p ) );
        return jumptable( p.segment );
    }

    MemoryBits memoryflag( Pointer p ) {
        ASSERT( owns( p ) );
        return MemoryBits( memory() + size_jumptable( segcount ),
                           offset( p ) );
    }

    bool inBounds( Pointer p, int off ) {
        p.offset += off; return dereference( p );
    }

    int offset( Pointer p ) {
        ASSERT( owns( p ) );
        return int( jumptable( p ) * 4 ) + p.offset;
    }

    int size( Pointer p ) {
        ASSERT( owns( p ) );
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
        ASSERT( owns( p ) );
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
        ASSERT_LEQ( segment + segshift, int( 1u << Pointer::segmentSize ) - 1 );
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
        ASSERT( owns( p ) );
        return offsets[ p.segment - segshift ] + p.offset;
    }

    int size( Pointer p ) {
        ASSERT( owns( p ) );
        return offsets[ p.segment - segshift + 1] - offsets[ p.segment - segshift ];
    }

    MemoryBits memoryflag( Pointer p ) {
        return MemoryBits( &flags.front(), offset( p ) );
    }

    char *dereference( Pointer p ) {
        ASSERT( owns( p ) );
        if ( offset( p ) >= offsets[ offsets.size() - 1 ] )
            return nullptr;
        ASSERT_LEQ( offsets[ p.segment - segshift ] + size( p ), offsets[ offsets.size() - 1 ] );
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

/* Doesn't store any heap metadata. */
struct NoHeapMeta {
    void setSize( int ) {}
    void copyFrom( NoHeapMeta &, int /* fromid */, int /* toid */ ) {}
    void newObject( int /* id */ ) {}
    StateAddress advance( StateAddress a, int ) { return a; }
    int end() { return 0; }
    static int size( int ) { return 0; }
    static std::vector< int > pointerId( ::llvm::Instruction * ) {
        return std::vector< int >();
    }
    int idAt( int ) { return 0; }
    std::string fmt() { return ""; }
};

/* Track heap object IDs (but nothing else). */
struct HeapIDs : WithMemory< HeapIDs > {
    int count;

    static int size( int cnt ) { return sizeof( HeapIDs ) + cnt * sizeof( int ); }

    StateAddress advance( StateAddress a, int ) {
        return StateAddress( a, 0, sizeof( HeapIDs ) + count * sizeof( int ) );
    }
    int end() { return 0; }

    int &idAt( int idx ) {
        ASSERT_LEQ( idx, count - 1 );
        return reinterpret_cast< int * >( memory() )[ idx ];
    }

    void setSize( int c ) { count = c; }
    void newObject( int id ) { idAt( count++ ) = id; }
    void copyFrom( HeapIDs &from, int fromid, int toid ) {
        idAt( toid ) = from.idAt( fromid );
    }

    static void badPointerId( ::llvm::Instruction *i ) {
        i->dump();
        ASSERT_UNREACHABLE( "Malformed aa_def metadata encountered." );
    }

    static std::vector< int > pointerId( ::llvm::Instruction *insn )
    {
        std::vector< int > r;

        auto md = insn->getMetadata( "aa_def" );

        if ( !md ) /* no metadata at all, carry on */
            return r;

        for ( int idx = 0; idx < int( md->getNumOperands() ); ++idx ) {

            auto memloc = ::llvm::cast< ::llvm::MDNode >( md->getOperand( idx ) );
            if ( !memloc )
                badPointerId( insn );

            auto id = ::llvm::cast< ::llvm::ConstantInt >( memloc->getOperand( 0 ) );
            if ( !id )
                badPointerId( insn );
            r.push_back( id->getZExtValue() ); /* downconvert ... */
        }

        return r;
    }

    int pointerId( Pointer p ) {
        return idAt( p.segment );
    }

    std::string fmt() {
        std::stringstream s;
        s << "[";
        for ( int i = 0; i < count; ++i )
            s << " " << idAt( i );
        s << " ]";
        return s.str();
    }
};

struct Flags : WithMemory< Flags >
{
    uint64_t problemcount:8;
    uint64_t ap:44;
    uint64_t buchi:12;
    Problem &problems( int i ) {
        return *( reinterpret_cast< Problem * >( this->memory() ) + i );
    }

    StateAddress advance( StateAddress a, int ) {
        return StateAddress( a, 0, sizeof( Flags ) + problemcount * sizeof( Problem ) );
    }
    int end() { return 0; }
};

inline int size_jumptable( int segcount ) {
    /* 4-align, extra item for "end of memory" */
    return 2 * (2 + segcount - segcount % 2);
}

inline int size_memoryflags( int bytecount ) {
    const int bitcount = bytecount * MemoryBits::bitwidth;
    return divine::align( bitcount / 8 + ((bitcount % 8) ? 1 : 0), 4 );
}

inline int size_heap( int segcount, int bytecount ) {
    return sizeof( Heap ) +
        bytecount +
        size_jumptable( segcount ) +
        size_memoryflags( bytecount );
}

}

template< typename _HeapMeta = machine::HeapIDs >
struct MachineState
{
    using HeapMeta = _HeapMeta;
    using Frame = machine::Frame;
    using Nursery = machine::Nursery;
    using StateAddress = machine::StateAddress;
    using Flags = machine::Flags;
    using Globals = machine::Globals;
    using Heap = machine::Heap;

    Blob _blob;
    char *_blob_deref;

    /* those stacks experienced an entry and fit in _blob no more */
    std::vector< std::pair< bool, Blob > > _stack;
    Nursery nursery;
    Blob _heapmeta; /* for objects in nursery */
    std::set< int > freed;
    std::vector< Problem > problems;

    ProgramInfo &_info;
    Pool &_pool;
    int _slack;
    int _thread; /* the currently scheduled thread */
    int _thread_count;
    Frame *_frame; /* pointer to the currently active frame */

    template< typename T >
    using Lens = lens::Lens< StateAddress, T >;

    typedef lens::Array< Frame > Stack;
    typedef lens::Array< Stack > Threads;
    typedef lens::Tuple< Flags, Globals, Heap, HeapMeta, Threads > State;

    using StateLens = Lens< State >;

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

    Pointer malloc( int size, int id ) {
        _pool.get< HeapMeta >( _heapmeta ).newObject( id );
        return nursery.malloc( size );
    }

    int pointerId( Pointer p ) {
        if ( nursery.owns( p ) ) {
            p.segment -= nursery.segshift;
            return _pool.get< HeapMeta >( _heapmeta ).idAt( p.segment );
        }
        if ( heap().owns( p ) )
            return state().get( HeapMeta() ).idAt( p.segment );
        return 0;
    }

    bool free( Pointer p ) {
        if ( p.null() )
            return true; /* nothing to do */
        if( !validate( p ) || !p.heap )
            return false;
        freed.insert( p.segment );
        return true;
    }

    bool isPrivate( int tid, Pointer p );
    bool isPrivate( Pointer p, Frame &, machine::Canonic< HeapMeta > & );
    bool isPrivate( Pointer p, Pointer, machine::Canonic< HeapMeta > & );

    StateLens state( Blob b ) {
        return Lens< State >( StateAddress( &_info, _pool.dereference( b ), _slack ) );
    }

    StateLens state() {
        return Lens< State >( StateAddress( &_info, _blob_deref, _slack ) );
    }

    MemoryBits memoryflag( Pointer p, int offset = 0 ) {
        p.offset += offset; /* beware of dragons! */
        if ( nursery.owns( p ) )
            return nursery.memoryflag( p );
        if ( heap().owns( p ) )
            return heap().memoryflag( p );
        if ( globalPointer( p ) )
            return global().memoryflag( _info, p );
        if( constantPointer( p ) )
            return MemoryBits();
        ASSERT_UNREACHABLE( "invalid pointer passed to memoryflags" );
    }

    MemoryBits memoryflag( ValueRef v ) {
        if ( v.tid < 0 )
            v.tid = _thread;
        if( v.v.constant )
            return MemoryBits();
        ASSERT( !v.v.global );
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
        ASSERT( !v.v.global );
        ASSERT( !v.v.constant );
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

        ASSERT_UNREACHABLE( "Impossible Value." );
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

    bool validate( Pointer p, bool h ) {
        bool hv = heap().owns( p ) || nursery.owns( p );
        return (h && hv) || (!h && !hv);
    }

    Lens< Threads > threads() {
        return state().sub( Threads() );
    }

    Lens< Stack > _blob_stack( int i ) {
        ASSERT_LEQ( 0, i );
        ASSERT_LEQ( i, threads().get().length() - 1 );
        return state().sub( Threads(), i );
    }

    void detach_stack( int thread, int needbytes ) {
        if ( int( _stack.size() ) <= thread )
            _stack.resize( thread + 1, std::make_pair( false, Blob() ) );

        StateAddress current, next;
        auto &ss = _stack[thread];
        Blob current_b = ss.second;

        if ( !_pool.valid( current_b ) )
            ss.second = _pool.allocate( std::max( 65500, needbytes ) );

        current = next = StateAddress( &_info, _pool.dereference( ss.second ), 0 );

        if ( !ss.first ) {
            if ( thread < threads().get().length() ) {
                current = state().address( Threads(), thread );
                current_b = _blob;
            }
            _pool.get< int >( ss.second ) = 0; /* clear any pre-existing state */
        }

        Lens< Stack > from( current );
        int neednow = lens::LinearSize< Stack >::get( current );

        if ( neednow + needbytes > _pool.size( ss.second ) ) {
            next = StateAddress( &_info, _pool.dereference(
                                     ss.second = _pool.allocate( neednow + needbytes ) ), 0 );
        }

        if ( current_b != ss.second ) {
            from.copy( next );
            if ( ss.first )
                _pool.free( current_b );
        }

        ss.first = true; /* activate */
    }

    Lens< Stack > stack( int thread = -1 )
    {
        if ( thread < 0 )
            thread = _thread;

        if ( thread < int( _stack.size() ) && _stack[thread].first )
            return Lens< Stack >( StateAddress( &_info, _pool.dereference( _stack[thread].second ), 0 ) );
        else
            return _blob_stack( thread );
    }

    HeapMeta &heapMeta() {
        return state().get( HeapMeta() );
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
    bool eachframe( Lens< Stack > s, F f ) {
        int idx = 0;
        auto address = s.sub( 0 ).address();
        while ( idx < s.get().length() ) {
            Frame &fr = address.template as< Frame >();
            if ( !f( fr ) )
                return false;
            address = fr.advance( address, 0 );
            ++ idx;
        }
        return true;
    }

    void switch_thread( int thread );
    int new_thread();

    int pointerSize( Pointer p );
    template< typename V >
    Pointer &followPointer( V p ) {
        return *reinterpret_cast< Pointer * >( dereference( p ) );
    }


    void trace( Pointer p, machine::Canonic< HeapMeta > &canonic );
    void trace( Frame &f, machine::Canonic< HeapMeta > &canonic );
    void snapshot( Pointer &edit, Pointer original, machine::Canonic< HeapMeta > &canonic, Heap &heap );
    void snapshot( Frame &f, machine::Canonic< HeapMeta > &canonic, Heap &heap, StateAddress &address );

    template< typename Alloc >
    void rewind( Alloc alloc, Blob to, int thread = 0 )
    {
        alloc.drop( _pool, _blob );
        _blob = alloc.get( _pool, _pool.size( to ) );
        _blob_deref = _pool.dereference( _blob );
        _pool.copy( to, _blob );

        _thread = -1; // special

        _thread_count = threads().get().length();
        nursery.reset( heap().segcount );
        _pool.get< HeapMeta >( _heapmeta ).setSize( 0 ),

        freed.clear();
        problems.clear();
        for ( int i = 0; i < int( _stack.size() ); ++i ) /* deactivate detached stacks */
            _stack[i].first = false;

        if ( thread >= 0 && thread < _thread_count )
            switch_thread( thread );
    }

    void problem( Problem::What, Pointer p = Pointer() );

    int size( int stack, int heapbytes, int heapsegs, int problems ) {
        return sizeof( Flags ) +
               sizeof( Problem ) * problems +
               HeapMeta::size( heapsegs ) +
               sizeof( int ) + /* thread count */
               stack + machine::size_heap( heapsegs, heapbytes ) + Globals::size( _info );
    }

    MachineState( ProgramInfo &i, Pool &pool, int slack )
        : _info( i ), _pool( pool ), _slack( slack )
    {
        _thread_count = 0;
        _frame = nullptr;
        nursery.reset( 0 ); /* nothing in the heap */
        _heapmeta = _pool.allocate( 4080 );
    }

    void dump( std::ostream & );
    void dump();

    template< typename Alloc >
    divine::Blob snapshot( Alloc alloc )
    {
        machine::Canonic< HeapMeta > canonic( *this );
        int dead_threads = 0;

        for ( auto var : _info.globalvars ) {
            ASSERT ( !var.second.constant );
            Pointer p( false, var.first, 0 );
            for ( p.offset = 0; p.offset < var.second.width; p.offset += 4 )
                if ( isHeapPointer( global().memoryflag( _info, p ) ) )
                    trace( followPointer( p ), canonic );
        }

        for ( int tid = 0; tid < _thread_count; ++tid ) {
            if ( !stack( tid ).get().length() ) { /* leave out dead threads */
                ++ dead_threads;
                continue;
            }

            /* non-tail dead threads become zombies, with 4 byte overhead; put them back */
            canonic.stack += dead_threads * sizeof( Stack );
            dead_threads = 0;
            canonic.stack += sizeof( Stack );
            eachframe( stack( tid ), [&]( Frame &fr ) {
                    this->trace( fr, canonic );
                    return true; // continue
                } );
        }

        /* we only store new problems, discarding those we inherited */
        for ( int i = 0; i < int( problems.size() ); ++i )
            if ( !problems[ i ].pointer.null() )
                trace( problems[ i ].pointer, canonic );

        Pointer p( true, 0, 0 );
        const uint32_t limit = heap().segcount + nursery.offsets.size() - 1;
        ASSERT_LEQ( limit, (1u << Pointer::segmentSize) - 1 );
        for ( p.segment = 0; p.segment < limit; ++ p.segment )
            if ( !canonic.seen( p ) && !freed.count( p.segment ) ) {
                trace( p, canonic );
                problem( Problem::MemoryLeak, p );
            }

        Blob b = alloc.get( _pool, _slack +
                            size( canonic.stack, canonic.allocated, canonic.segcount, problems.size() ) );
        _pool.clear( b );

        StateAddress address( &_info, _pool.dereference( b ), _slack );
        Flags &fl = address.as< Flags >();
        fl = flags();
        fl.problemcount = problems.size();
        address.advance( sizeof( Flags ) );

        for ( int i = 0; i < int( problems.size() ); ++i ) {
            address.advance( sizeof( Problem ) );
            fl.problems( i ) = problems[ i ];
        }

        Globals *_global = &address.as< Globals >();
        address = state().sub( Globals() ).copy( address );

        /* skip the heap */
        Heap *_heap = &address.as< Heap >();
        _heap->segcount = canonic.segcount;
        /* heap needs to know its size in order to correctly dereference! */
        _heap->jumptable( canonic.segcount ) = canonic.allocated / 4;
        address.advance( machine::size_heap( canonic.segcount, canonic.allocated ) );
        ASSERT_EQ( machine::size_heap( canonic.segcount, canonic.allocated ) % 4, 0 );

        auto &heapmeta = address.as< HeapMeta >();
        heapmeta.setSize( canonic.segcount );
        address = heapmeta.advance( address, 0 );

        address.as< int >() = _thread_count - dead_threads;
        address.advance( sizeof( int ) ); // ick. length of the threads array

        for ( auto var : _info.globalvars ) {
            Pointer p( false, var.first, 0 );
            ASSERT( !var.second.constant );
            for ( p.offset = 0; p.offset < var.second.width; p.offset += 4 )
                if ( isHeapPointer( global().memoryflag( _info, p ) ) )
                    snapshot( *(_global->dereference< Pointer >( _info, p )),
                              followPointer( p ), canonic, *_heap );
        }

        for ( int tid = 0; tid < _thread_count - dead_threads; ++tid ) {
            address.as< int >() = stack( tid ).get().length();
            address.advance( sizeof( int ) );
            eachframe( stack( tid ), [&]( Frame &fr ) {
                    snapshot( fr, canonic, *_heap, address );
                    return true; // continue
                });
        }

        for ( int i = 0; i < fl.problemcount; ++i ) {
            auto &p = fl.problems( i ).pointer;
            if ( !p.null() )
                snapshot( p, p, canonic, *_heap );
        }

        auto &nursery_hm = _pool.get< HeapMeta >( _heapmeta ),
              &mature_hm = state().get( HeapMeta() );

        for ( int seg = 0; seg < int( canonic.segmap.size() ); ++seg ) {
            if ( canonic.segmap[ seg ] < 0 )
                continue;
            bool nursed = seg >= nursery.segshift;
            heapmeta.copyFrom( nursed ? nursery_hm : mature_hm,
                               seg - (nursed ? nursery.segshift : 0), canonic.segmap[ seg ] );
        }

        ASSERT_EQ( canonic.segdone, canonic.segcount );
        ASSERT_EQ( canonic.boundary, canonic.allocated );
        ASSERT_EQ( address.offset, _pool.size( b ) );
        ASSERT_EQ( (_pool.size( b ) - _slack) % 4, 0 );

        return b;
    }
};

struct FrameContext {
    ProgramInfo &info;
    machine::Frame &frame;

    template< typename X > MemoryBits memoryflag( X x ) { return frame.memoryflag( info, x ); }
    template< typename X > char *dereference( X x ) { return frame.dereference( info, x ); }
    template< typename X > bool validate( X, bool ) { return true; }
    template< typename X > bool inBounds( X x, int off ) {
        x.offset += off;
        return dereference( x );
    }

    FrameContext( ProgramInfo &i, machine::Frame &f )
        : info( i ), frame( f )
    {}
};


}
}

namespace std {

inline std::string to_string( divine::llvm::Problem::What p ) {
    using P = divine::llvm::Problem::What;
    #define PROBLEM(x) if ( p == P::x ) return #x;
    #include <divine/llvm/problem.def>
    #undef PROBLEM
    return "<<UNKNOWN>>";
}

}

#endif

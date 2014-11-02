// -*- C++ -*- (c) 2011-2014 Petr Rockai <me@mornfall.net>

#define NO_RTTI
#include <divine/llvm/machine.h>

using namespace divine::llvm;

template< typename HeapMeta >
void MachineState< HeapMeta >::rewind( Blob to, int thread )
{
    _pool.free( _blob );
    _blob = _pool.allocate( _pool.size( to ) );
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

template< typename HeapMeta >
void MachineState< HeapMeta >::switch_thread( int thread )
{
    _thread = thread;
    if ( stack().get().length() )
        _frame = &stack().get( stack().get().length() - 1 );
    else
        _frame = nullptr;
}

template< typename HeapMeta >
int MachineState< HeapMeta >::new_thread()
{
    detach_stack( _thread_count, 0 );
    _thread = _thread_count ++;
    stack().get().length() = 0;
    return _thread;
}

template< typename HeapMeta >
int MachineState< HeapMeta >::pointerSize( Pointer p )
{
    if ( !validate( p ) )
        return 0;

    /*
     * Pointers into global memory are not used for copying during
     * canonization, so we don't actually need to know their size.
     */
    ASSERT( !globalPointer( p ) );

    if ( heap().owns( p ) )
        return heap().size( p );
    else
        return nursery.size( p );
}

template< typename HeapMeta >
struct divine::llvm::Canonic
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

template< typename HeapMeta >
void MachineState< HeapMeta >::trace( Pointer p, Canonic< HeapMeta > &canonic )
{
    if ( p.heap && !freed.count( p.segment ) && !canonic.seen( p ) ) {
        int size = pointerSize( p );
        canonic[ p ];
        for ( p.offset = 0; p.offset < size; p.offset += 4 )
            if ( isHeapPointer( memoryflag( p ) ) )
                trace( followPointer( p ), canonic );
    }
}

template< typename Fun >
void forPointers( machine::Frame &f, ProgramInfo &i, ValueRef v, Fun fun )
{
    if ( isHeapPointer( f.memoryflag( i, v ) ) )
        fun( v, *f.dereference< Pointer >( i, v ) );
/*
    while ( v.offset < v.v.width - 4 ) {
        v.offset += 4;
        if ( f.isPointer( i, v ) )
            fun( v, *f.dereference< Pointer >( i, v ) );
    }
*/
}

template< typename HeapMeta >
void MachineState< HeapMeta >::trace( Frame &f, Canonic< HeapMeta > &canonic )
{
    auto vals = _info.function( f.pc ).values;
    for ( auto val : vals )
        forPointers( f, _info, val, [&]( ValueRef, Pointer p ) {
                this->trace( p, canonic );
            } );
    canonic.stack += sizeof( f ) + f.framesize( _info );
}

template< typename HeapMeta >
void MachineState< HeapMeta >::snapshot(
    Pointer &edit, Pointer original, Canonic< HeapMeta > &canonic, Heap &heap )
{
    if ( !original.heap ) { /* non-heap pointers are always canonic */
        edit = original;
        return;
    }

    /* clear invalid pointers, in case they would accidentally become valid later */
    if ( !validate( original ) ) {
        if ( original.null() )
            edit = Pointer( false, 0, 0 );
        else
            edit = Pointer( false, 0, 1 ); /* not NULL, so we can detect an invalid free */
        return;
    }

    ASSERT( canonic.seen( original ) );
    edit = canonic[ original ]; /* canonize */

    if ( edit.segment < canonic.segdone )
        return; /* we already followed this pointer */

    Pointer edited = edit;
    ASSERT_EQ( int( edit.segment ), canonic.segdone );
    canonic.segdone ++;

    /* Re-allocate the object... */
    int size = pointerSize( original );
    heap.jumptable( edit ) = canonic.boundary / 4;
    canonic.boundary += size;
    heap.jumptable( edit.segment + 1 ) = canonic.boundary / 4;

    /* And trace it. We work in 4 byte steps, pointers are 4 bytes and 4-byte aligned. */
    original.offset = edited.offset = 0;
    for ( ; original.offset < size; original.offset += 4, edited.offset += 4 ) {
        auto origflag = this->memoryflag( original );
        if ( isHeapPointer( origflag ) )
            heap.memoryflag( edited ).set( origflag.get() );
        if ( isHeapPointer( origflag ) ) /* points to a pointer, recurse */
            snapshot( *heap.dereference< Pointer >( edited ),
                      followPointer( original ), canonic, heap );
        else
            memcopy_fastpath( original, edited, 4, *this, heap );
    }
}

template< typename HeapMeta >
void MachineState< HeapMeta >::snapshot(
    Frame &f, Canonic< HeapMeta > &canonic, Heap &heap, StateAddress &address )
{
    auto vals = _info.function( f.pc ).values;
    Frame &target = address.as< Frame >();
    target.pc = f.pc;

    ProgramInfo::Value zero;
    /* make a straight copy first, we will rewrite pointers next */
    FrameContext fctx( _info, f ), tctx( _info, target );
    memcopy_fastpath( zero, zero, target.datasize( _info ), fctx, tctx );

    for ( auto val = vals.begin(); val != vals.end(); ++val ) {
        forPointers( f, _info, *val, [&]( ValueRef v, Pointer p ) {
                target.memoryflag( _info, v ).set( f.memoryflag( _info, v ).get() );
                this->snapshot( *target.dereference< Pointer >( _info, v ), p, canonic, heap );
            } );
    }

    address = target.advance( address, 0 );
    ASSERT_EQ( (address.offset - _slack) % 4, 0 );
}

template< typename HeapMeta >
divine::Blob MachineState< HeapMeta >::snapshot()
{
    Canonic< HeapMeta > canonic( *this );
    int dead_threads = 0;

    for ( auto var : _info.globalvars ) {
        assert ( !var.second.constant );
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

    Blob b = _pool.allocate( _slack +
        size( canonic.stack, canonic.allocated, canonic.segcount, problems.size() ) );
    _pool.clear( b );

    StateAddress address( &_pool, &_info, b, _slack );
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


template< typename HeapMeta >
void MachineState< HeapMeta >::problem( Problem::What w, Pointer ptr )
{
    Problem p;
    p.what = w;
    if ( _frame )
        p.where = frame().pc;
    p.tid = _thread;
    p.pointer = ptr;
    problems.push_back( p );
}

template< typename HeapMeta >
bool MachineState< HeapMeta >::isPrivate( Pointer needle, Pointer p, Canonic< HeapMeta > &canonic )
{
    if ( p.heap && needle.segment == p.segment )
        return false;

    if ( canonic.seen( p ) )
        return true;

    canonic[p];

    if ( p.heap && !freed.count( p.segment ) ) {
        int size = pointerSize( p );
        for ( p.offset = 0; p.offset < size; p.offset += 4 )
            if ( isHeapPointer( memoryflag( p ) ) )
                if ( !isPrivate( needle, followPointer( p ), canonic ) )
                    return false;
    }

    return true;
}

template< typename HeapMeta >
bool MachineState< HeapMeta >::isPrivate( Pointer needle, Frame &f, Canonic< HeapMeta > &canonic )
{
    bool result = true;

    auto vals = _info.function( f.pc ).values;
    for ( auto val : vals ) {
        forPointers( f, _info, val, [&]( ValueRef, Pointer p ) {
                if ( !isPrivate( needle, p, canonic ) )
                    result = false;
            } );
        if ( !result ) break; /* bail early */
    }
    return result;
}

template< typename HeapMeta >
bool MachineState< HeapMeta >::isPrivate( int tid, Pointer needle )
{
    if ( !needle.heap ) /* globals are never private */
        return _thread_count <= 1;


    Canonic< HeapMeta > canonic( *this );

    for ( auto var : _info.globalvars ) {
        Pointer p( false, var.first, 0 );
        ASSERT( !var.second.constant );
        for ( p.offset = 0; p.offset < var.second.width; p.offset += 4 )
            if ( isHeapPointer( global().memoryflag( _info, p ) ) )
                if ( !isPrivate( needle, followPointer( p ), canonic ) )
                    return false;
    }

    for ( int search = 0; search < _thread_count; ++search ) {
        if ( tid == search )
            continue;
        if ( !eachframe( stack( search ), [&]( Frame &fr ) {
                    if ( !isPrivate( needle, fr, canonic ) )
                        return false;
                    return true;
                } ) )
            return false;
    }

    return true; /* not found */
}

namespace divine {
namespace llvm {

/* explicit instances */
template struct MachineState< machine::NoHeapMeta >;
template struct MachineState< machine::HeapIDs >;

}
}

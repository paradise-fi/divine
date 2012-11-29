// -*- C++ -*- (c) 2011, 2012 Petr Rockai <me@mornfall.net>

#include <divine/llvm/machine.h>

using namespace divine::llvm;

void MachineState::rewind( Blob to, int thread )
{
    if ( _blob_private && _blob.valid() )
        _blob.free( _alloc.pool() );

    _blob_private = false;
    _blob = to;
    _thread = -1; // special

    _thread_count = threads().get().length();
    nursery.reset( heap().segcount );

    if ( thread >= 0 && thread < threads().get().length() )
        switch_thread( thread );
}

void MachineState::resnap()
{
    Blob snap = snapshot();

    if ( _blob_private && _blob.valid() )
        _blob.free( _alloc.pool() );

    _blob_private = true;
    _blob = snap;
}

void MachineState::switch_thread( int thread )
{
    resnap();

    assert_leq( thread, threads().get().length() - 1 );
    _thread = thread;

    StateAddress stackaddr( &_info, _stack, 0 );
    _blob_stack( _thread ).copy( stackaddr );
    assert( stack().get().length() );
    _frame = &stack().get( stack().get().length() - 1 );
}

int MachineState::new_thread()
{
    resnap();
    _thread = _thread_count ++;
    stack().get().length() = 0;
    return _thread;
}

Pointer MachineState::followPointer( Pointer p )
{
    if ( !validate( p ) )
        return Pointer();

    if ( globalPointer( p ) )
        return Pointer(); /* TODO! */

    Pointer next = *reinterpret_cast< Pointer * >( dereference( p ) );
    if ( heap().owns( p ) ) {
        if ( heap().isPointer( p ) )
            return next;
    } else if ( nursery.isPointer( p ) )
        return next;
    return Pointer();
}

int MachineState::pointerSize( Pointer p )
{
    if ( !validate( p ) )
        return 0;

    /*
     * Pointers into global memory are not used for copying during
     * canonization, so we don't actually need to know their size.
     */
    assert( !globalPointer( p ) );

    if ( heap().owns( p ) )
        return heap().size( p );
    else
        return nursery.size( p );
}

struct divine::llvm::Canonic
{
    MachineState &ms;
    std::map< int, int > segmap;
    int allocated, segcount;
    int stack;
    int boundary, segdone;

    Canonic( MachineState &ms )
        : ms( ms ), allocated( 0 ), segcount( 0 ), stack( 0 ), boundary( 0 ), segdone( 0 )
    {}

    Pointer operator[]( Pointer idx ) {
        if ( !idx.heap )
            return idx;
        if ( !segmap.count( idx.segment ) ) {
            segmap.insert( std::make_pair( int( idx.segment ), segcount ) );
            ++ segcount;
            allocated += ms.pointerSize( idx );
        }
        return Pointer( idx.heap, segmap[ int( idx.segment ) ], idx.offset );
    }
};

void MachineState::trace( Pointer p, Canonic &canonic )
{
    if ( p.heap ) {
        canonic[ p ];
        trace( followPointer( p ), canonic );
    }
}

void MachineState::trace( Frame &f, Canonic &canonic )
{
    auto vals = _info.function( f.pc ).values;
    for ( auto val = vals.begin(); val != vals.end(); ++val ) {
        if ( val->pointer() )
            trace( *reinterpret_cast< Pointer * >( &f.memory[val->offset] ), canonic );
    }
    canonic.stack += sizeof( f ) + f.framesize( _info );
}



void MachineState::snapshot( Pointer from, Pointer to, Canonic &canonic, Heap &heap )
{
    if ( !validate( from ) || !from.heap || to.segment < canonic.segdone )
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

void MachineState::snapshot( Frame &f, Canonic &canonic, Heap &heap, StateAddress &address )
{
    auto vals = _info.function( f.pc ).values;
    Frame &target = address.as< Frame >();
    target.pc = f.pc;

    for ( auto val = vals.begin(); val != vals.end(); ++val ) {
        char *from_addr = f.dereference( _info, *val );
        if ( val->pointer() ) {
            Pointer from = *reinterpret_cast< Pointer * >( from_addr );
            Pointer to = canonic[ from ];
            *target.dereference< Pointer >( _info, *val ) = to;
            snapshot( from, to, canonic, heap );
        } else
            std::copy( from_addr, from_addr + val->width, target.dereference( _info, *val ) );
    }

    address = target.advance( address, 0 );
    assert_eq( address.offset % 4, 0 );
}

divine::Blob MachineState::snapshot()
{
    Canonic canonic( *this );
    int dead_threads = 0;

    /* TODO trace the global memory! */

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
                trace( fr, canonic );
            } );
    }

    Blob b = _alloc.new_blob(
        size( canonic.stack, canonic.allocated, canonic.segcount ) );

    StateAddress address( &_info, b, _alloc._slack );
    address = state().sub( Flags() ).copy( address );
    assert_eq( address.offset, _alloc._slack + sizeof( Flags ) );
    address = state().sub( Globals() ).copy( address ); /* TODO run through canonizer, too! */

    /* skip the heap */
    Heap *_heap = &address.as< Heap >();
    _heap->segcount = canonic.segcount;
    /* heap needs to know its size in order to correctly dereference! */
    _heap->jumptable( canonic.segcount ) = canonic.allocated / 4;
    address.advance( size_heap( canonic.segcount, canonic.allocated ) );
    assert_eq( size_heap( canonic.segcount, canonic.allocated ) % 4, 0 );

    address.as< int >() = _thread_count - dead_threads;
    address.advance( sizeof( int ) ); // ick. length of the threads array

    for ( int tid = 0; tid < _thread_count - dead_threads; ++tid ) {
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


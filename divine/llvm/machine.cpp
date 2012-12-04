// -*- C++ -*- (c) 2011, 2012 Petr Rockai <me@mornfall.net>

#include <divine/llvm/machine.h>

using namespace divine::llvm;

void MachineState::rewind( Blob to, int thread )
{
    _blob.free( _alloc.pool() );
    _blob = Blob( _alloc.pool(), to.size() );
    to.copyTo( _blob );

    _thread = -1; // special

    _thread_count = threads().get().length();
    nursery.reset( heap().segcount );
    freed.clear();
    problems.clear();
    for ( int i = 0; i < int( _stack.size() ); ++i ) /* deactivate detached stacks */
        _stack[i].first = false;

    if ( thread >= 0 && thread < _thread_count )
        switch_thread( thread );

}

void MachineState::switch_thread( int thread )
{
    _thread = thread;
    _frame = &stack().get( stack().get().length() - 1 );
}

int MachineState::new_thread()
{
    detach_stack( _thread_count );
    _thread = _thread_count ++;
    stack().get().length() = 0;
    return _thread;
}

Pointer &MachineState::followPointer( Pointer p ) {
    return *reinterpret_cast< Pointer * >( dereference( p ) );
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
        if ( !idx.heap || !ms.validate( idx ) )
            return idx;
        if ( !segmap.count( idx.segment ) ) {
            segmap.insert( std::make_pair( int( idx.segment ), segcount ) );
            ++ segcount;
            allocated += ms.pointerSize( idx );
        }
        return Pointer( idx.heap, segmap[ int( idx.segment ) ], idx.offset );
    }

    bool seen( Pointer p ) {
        if ( segmap.count( p.segment ) )
            return true;
    }
};

void MachineState::trace( Pointer p, Canonic &canonic )
{
    if ( p.heap && !freed.count( p.segment ) && !canonic.seen( p ) ) {
        int size = pointerSize( p );
        canonic[ p ];
        for ( p.offset = 0; p.offset < size; p.offset += 4 )
            if ( isPointer( p ) ) {
                trace( followPointer( p ), canonic );
            }
    }
}

void MachineState::trace( Frame &f, Canonic &canonic )
{
    auto vals = _info.function( f.pc ).values;
    for ( auto val = vals.begin(); val != vals.end(); ++val ) {
        if ( f.isPointer( _info, *val ) )
            trace( *f.dereference< Pointer >( _info, *val ), canonic );
    }
    canonic.stack += sizeof( f ) + f.framesize( _info );
}

void MachineState::snapshot( Pointer &edit, Pointer original, Canonic &canonic, Heap &heap )
{
    if ( !original.heap || freed.count( original.segment ) )
        return; /* do not touch */

    /* clear invalid pointers, in case they would accidentally become valid later */
    if ( !validate( original ) ) {
        edit = Pointer();
        return;
    }

    edit = canonic[ original ]; /* canonize */

    if ( edit.segment < canonic.segdone )
        return; /* we already followed this pointer */

    Pointer edited = edit;
    assert_eq( int( edit.segment ), canonic.segdone );
    canonic.segdone ++;

    /* Re-allocate the object... */
    int size = pointerSize( original );
    heap.jumptable( edit ) = canonic.boundary / 4;
    canonic.boundary += size;

    /* And trace it. We work in 4 byte steps, pointers are 4 bytes and 4-byte aligned. */
    original.offset = edited.offset = 0;
    for ( ; original.offset < size; original.offset += 4, edited.offset += 4 ) {
        bool recurse = this->isPointer( original );
        heap.setPointer( edited, recurse );
        if ( recurse ) /* points to a pointer, recurse */
            snapshot( *heap.dereference< Pointer >( edited ),
                      followPointer( original ), canonic, heap );
        else
            std::copy( dereference( original ), dereference( original ) + 4,
                       heap.dereference( edited ) );
    }
}

void MachineState::snapshot( Frame &f, Canonic &canonic, Heap &heap, StateAddress &address )
{
    auto vals = _info.function( f.pc ).values;
    Frame &target = address.as< Frame >();
    target.pc = f.pc;

    for ( auto val = vals.begin(); val != vals.end(); ++val ) {
        char *from_addr = f.dereference( _info, *val );
        bool recurse = f.isPointer( _info, *val );
        target.setPointer( _info, *val, recurse );
        if ( recurse ) {
            Pointer from = *reinterpret_cast< Pointer * >( from_addr );
            snapshot( *target.dereference< Pointer >( _info, *val ), from, canonic, heap );
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

    /* TODO inefficient, split globals into globals and constants */
    for ( int i = 0; i < int( _info.globals.size() ); ++i ) {
        ProgramInfo::Value v = _info.globals[i];
        if ( v.constant )
            continue;
        Pointer p( false, i, 0 );
        for ( p.offset = 0; p.offset < v.width; p.offset += 4 )
            if ( global().isPointer( _info, p ) )
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
                trace( fr, canonic );
            } );
    }

    int problemcount = flags().problemcount + problems.size();
    Blob b = _alloc.new_blob(
        size( canonic.stack, canonic.allocated, canonic.segcount,
              flags().problem ? problemcount : 0 ) );

    StateAddress address( &_info, b, _alloc._slack );
    Flags &fl = address.as< Flags >();
    fl = flags();
    fl.problemcount = 0;
    address.advance( sizeof( Flags ) );

    if ( fl.problem ) {
        for ( int i = 0; i < problemcount; ++i ) {
            address.advance( sizeof( Problem ) );
            fl.problems[ i ] =
                i < flags().problemcount ?
                    flags().problems[ i ] :
                    problems[ i - flags().problemcount ];
        }
        fl.problemcount = problemcount;
    }

    Globals *_global = &address.as< Globals >();
    address = state().sub( Globals() ).copy( address );

    /* skip the heap */
    Heap *_heap = &address.as< Heap >();
    _heap->segcount = canonic.segcount;
    /* heap needs to know its size in order to correctly dereference! */
    _heap->jumptable( canonic.segcount ) = canonic.allocated / 4;
    address.advance( size_heap( canonic.segcount, canonic.allocated ) );
    assert_eq( size_heap( canonic.segcount, canonic.allocated ) % 4, 0 );

    address.as< int >() = _thread_count - dead_threads;
    address.advance( sizeof( int ) ); // ick. length of the threads array

    for ( int i = 0; i < int( _info.globals.size() ); ++i ) {
        ProgramInfo::Value v = _info.globals[i];
        if ( v.constant )
            continue;
        Pointer p( false, i, 0 );
        for ( p.offset = 0; p.offset < v.width; p.offset += 4 )
            if ( global().isPointer( _info, p ) )
                snapshot( *(_global->dereference< Pointer >( _info, p )),
                          followPointer( p ), canonic, *_heap );
    }

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


void MachineState::problem( Problem::What w )
{
    Problem p;
    p.what = w;
    p.where = frame().pc;
    p.tid = _thread;
    problems.push_back( p );
    flags().problem = 1;
}

bool MachineState::isPrivate( Pointer needle, Pointer p, Canonic &canonic )
{
    if ( p.heap && needle.segment == p.segment )
        return false;

    if ( canonic.seen( p ) )
        return true;

    canonic[p];

    if ( p.heap && !freed.count( p.segment ) ) {
        int size = pointerSize( p );
        for ( p.offset = 0; p.offset < size; p.offset += 4 )
            if ( isPointer( p ) ) {
                if ( !isPrivate( needle, followPointer( p ), canonic ) )
                    return false;
            }
    }

    return true;
}

bool MachineState::isPrivate( Pointer needle, Frame &f, Canonic &canonic )
{
    auto vals = _info.function( f.pc ).values;
    for ( auto val = vals.begin(); val != vals.end(); ++val ) {
        if ( f.isPointer( _info, *val ) )
            if ( !isPrivate( needle, *f.dereference< Pointer >( _info, *val ), canonic ) )
                return false;
    }
    return true;
}

bool MachineState::isPrivate( int tid, Pointer needle )
{
    if ( !needle.heap ) /* globals are never private */
        return false;

    bool found = false;

    Canonic canonic( *this );

    for ( int i = 0; i < int( _info.globals.size() ); ++i ) {
        ProgramInfo::Value v = _info.globals[i];
        if ( v.constant )
            continue;
        Pointer p( false, i, 0 );
        for ( p.offset = 0; p.offset < v.width; p.offset += 4 )
            if ( global().isPointer( _info, p ) )
                if ( !isPrivate( needle, followPointer( p ), canonic ) )
                    return false;
    }

    struct Found {};

    try {
        for ( int search = 0; search < _thread_count; ++search ) {
            if ( tid == search || found )
                continue;
            eachframe( stack( search ), [&]( Frame &fr ) {
                    if ( !isPrivate( needle, fr, canonic ) )
                        throw Found();
                } );
        }
    } catch ( Found ) {
        return false;
    }

    return true; /* not found */
}

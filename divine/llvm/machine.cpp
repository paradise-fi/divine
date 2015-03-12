// -*- C++ -*- (c) 2011-2014 Petr Rockai <me@mornfall.net>

#define NO_RTTI
#include <divine/llvm/machine.h>

using namespace divine::llvm;

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
void MachineState< HeapMeta >::trace( Pointer p, machine::Canonic< HeapMeta > &canonic )
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
void MachineState< HeapMeta >::trace( Frame &f, machine::Canonic< HeapMeta > &canonic )
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
    Pointer &edit, Pointer original, machine::Canonic< HeapMeta > &canonic, Heap &heap )
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
    Frame &f, machine::Canonic< HeapMeta > &canonic, Heap &heap, StateAddress &address )
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
bool MachineState< HeapMeta >::isPrivate( Pointer needle, Pointer p, machine::Canonic< HeapMeta > &canonic )
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
bool MachineState< HeapMeta >::isPrivate( Pointer needle, Frame &f, machine::Canonic< HeapMeta > &canonic )
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


    machine::Canonic< HeapMeta > canonic( *this );

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

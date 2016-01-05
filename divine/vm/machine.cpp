// -*- C++ -*- (c) 2011-2014 Petr Rockai <me@mornfall.net>

#include <divine/llvm/machine.hpp>

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
    while ( v.offset < v.v.width ) {
        if ( isHeapPointer( f.memoryflag( i, v ) ) )
            fun( v, *f.dereference< Pointer >( i, v ) );
        v.offset += 4;
    }
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
        if ( isHeapPointer( origflag ) ) { /* points to a pointer, recurse */
            auto editedflag = heap.memoryflag( edited );
            for ( int i = 0; i < 4; ++i, editedflag++, origflag++ )
                editedflag.set( origflag.get() );
            snapshot( *heap.dereference< Pointer >( edited ),
                      followPointer( original ), canonic, heap );
        } else
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
bool MachineState< HeapMeta >::isPrivate( int tid, Pointer needle )
{
    if ( !needle.heap ) /* globals are never private */
        return _thread_count <= 1;

    if ( _isPrivateTid >= 0 ) {
        ASSERT_EQ( _isPrivateTid, tid );
        if ( _isPrivate.count( needle.segment ) )
            return _isPrivate.find( needle.segment )->second;
        else
            return _isPrivate[ needle.segment ] = true;
    }

    _isPrivateTid = tid;

    for ( auto var : _info.globalvars ) {
        Pointer p( false, var.first, 0 );
        ASSERT( !var.second.constant );
        for ( p.offset = 0; p.offset < var.second.width; p.offset += 4 )
            if ( isHeapPointer( global().memoryflag( _info, p ) ) )
                markPublic( followPointer( p ).segment );
    }

    for ( int search = 0; search < _thread_count; ++search ) {
        if ( tid == search )
            continue;
        eachframe( stack( search ), [&]( Frame &fr ) {
                for ( auto &val : _info.function( fr.pc ).values )
                    forPointers( fr, _info, val, [&]( ValueRef, Pointer p ) {
                            markPublic( p.segment );
                        } );
                return true;
            } );
    }

    return isPrivate( tid, needle );
}

template< typename HeapMeta >
void MachineState< HeapMeta >::markPublic( int seg )
{
    if ( _isPrivate.count( seg ) )
        return;

    _isPrivate[ seg ] = false;
    Pointer p( true, seg, 0 );
    int size = pointerSize( p );
    for ( p.offset = 0; p.offset < size; p.offset += 4 )
        if ( isHeapPointer( memoryflag( p ) ) )
            markPublic( followPointer( p ).segment );
}

namespace divine {
namespace llvm {

/* explicit instances */
template struct MachineState< machine::NoHeapMeta >;
template struct MachineState< machine::HeapIDs >;

}
}

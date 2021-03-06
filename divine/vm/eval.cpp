// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2018 Petr Ročkai <code@fixp.eu>
 * (c) 2015 Vladimír Štill
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <divine/vm/eval.hpp>
#include <divine/vm/eval.tpp>
#include <divine/vm/memory.hpp>
#include <divine/vm/t-eval.hpp> /* for t_vm::TContext */

#ifdef __OpenBSD__
extern "C" long __syscall( long, ... );
#define syscall __syscall
#endif

namespace divine::vm
{

long syscall_helper( int id, std::vector< long > args, std::vector< bool > argtypes )
{
    using A = std::vector< bool >;

    if ( argtypes == A{} )
        return syscall( id );
    else if ( argtypes == A{0} )
        return syscall( id, int( args[0] ) );
    else if ( argtypes == A{1} )
        return syscall( id, args[0] );
    else if ( argtypes == A{0, 0} )
        return syscall( id, int( args[0] ), int( args[1] ) );
    else if ( argtypes == A{0, 1} )
        return syscall( id, int( args[0] ), args[1] );
    else if ( argtypes == A{1, 0} )
        return syscall( id, args[0] , int( args[1] ));
    else if ( argtypes == A{1, 1} )
        return syscall( id, args[0], args[1] );
    else if ( argtypes == A{0, 0, 0} )
        return syscall( id, int( args[0] ), int( args[1] ), int( args[2] ) );
    else if ( argtypes == A{0, 0, 1} )
        return syscall( id, int( args[0] ), int( args[1] ), args[2] );
    else if ( argtypes == A{0, 1, 0} )
        return syscall( id, int( args[0] ), args[1], int( args[2] ) );
    else if ( argtypes == A{0, 1, 1} )
        return syscall( id, int( args[0] ), args[1], args[2] );
    else if ( argtypes == A{1, 0, 0} )
        return syscall( id, args[0], int( args[1] ), int( args[2] ) );
    else if ( argtypes == A{1, 0, 1} )
        return syscall( id, args[0], int( args[1] ), args[2] );
    else if ( argtypes == A{1, 1, 0} )
        return syscall( id, args[0], args[1], int( args[2] ) );
    else if ( argtypes == A{1, 1, 1} )
        return syscall( id, args[0], args[1], args[2] );
    else if ( argtypes == A{0, 1, 1, 0, 1} )
        return syscall( id, int(args[0]), args[1], args[2], int(args[3]), args[4] );
    else if ( argtypes == A{0, 1, 1, 0, 1, 1} )
        return syscall( id, int(args[0]), args[1], args[2], int(args[3]), args[4], args[5] );
    else if ( argtypes == A{0, 1, 1, 0, 1, 0} )
        return syscall( id, int(args[0]), args[1], args[2], int(args[3]), args[4], int(args[5]) );
    else
        UNREACHABLE( "not implemented", argtypes );
}

template struct Eval< Context< Program, CowHeap > >;
template struct Eval< Context< Program, MutableHeap > >;
template struct Eval< Context< Program, SmallHeap > >;
template struct Eval< t_vm::TContext< Program > >;

}

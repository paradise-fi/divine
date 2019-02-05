// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016-2018 Petr Ročkai <code@fixp.eu>
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

#pragma once
#include <divine/smt/solver.hpp>

namespace divine::mc::impl
{

    template< typename Solver >
    struct Hasher
    {
        using Snapshot = vm::CowHeap::Snapshot;
        using Pool = vm::CowHeap::Pool;

        mutable vm::CowHeap _h1, _h2;
        vm::HeapPointer _root;
        Solver *_solver = nullptr;
        Pool *_pool = nullptr;

        void setup( Pool &pool, const vm::CowHeap &heap, Solver &solver )
        {
            _pool = &pool;
            _h1 = heap;
            _h2 = heap;
            _solver = &solver;
        }

        bool equal_fastpath( Snapshot a, Snapshot b ) const
        {
            bool rv = false;
            if ( _pool->size( a ) == _pool->size( b ) )
                rv = std::equal( _h1.snap_begin( *_pool, a ), _h1.snap_end( *_pool, a ),
                                 _h1.snap_begin( *_pool, b ) );
            if ( !rv )
                _h1.restore( *_pool, a ), _h2.restore( *_pool, b );
            return rv;
        }

        bool equal_explicit( Snapshot a, Snapshot b ) const
        {
            if ( equal_fastpath( a, b ) )
                return true;
            else
                return mem::compare( _h1, _h2, _root, _root ) == 0;
        }

        bool equal_symbolic( Snapshot a, Snapshot b ) const
        {
            if ( equal_fastpath( a, b ) )
                return true;


            struct Cmp : mem::NoopCmp< vm::HeapPointer >
            {
                std::vector< std::pair< vm::HeapPointer, vm::HeapPointer > > pairs;

                void marked( vm::HeapPointer a, vm::HeapPointer b )
                {
                    pairs.emplace_back( a, b );
                };
            } extract;

            if ( mem::compare( _h1, _h2, _root, _root, extract ) != 0 )
                return false;

            if ( extract.pairs.empty() )
                return true;

            ASSERT( _solver );
            return _solver->equal( extract.pairs, _h1, _h2 );
        }

        brick::hash::hash128_t hash( Snapshot s ) const
        {
            _h1.restore( *_pool, s );
            return mem::hash( _h1, _root );
        }
    };
}

namespace divine::mc
{

    using Snapshot = vm::CowHeap::Snapshot;

    template< typename Solver >
    struct Hasher : impl::Hasher< Solver >
    {
        bool equal( Snapshot a, Snapshot b ) const { return this->equal_symbolic( a, b ); }
    };

    template<>
    struct Hasher< smt::NoSolver > : impl::Hasher< smt::NoSolver >
    {
        bool equal( Snapshot a, Snapshot b ) const { return this->equal_explicit( a, b ); }
    };

}

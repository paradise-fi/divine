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

    struct PairExtract : mem::NoopCmp< vm::HeapPointer >
    {
        std::vector< std::pair< vm::HeapPointer, vm::HeapPointer > > pairs;

        void marked( vm::HeapPointer a, vm::HeapPointer b )
        {
            pairs.emplace_back( a, b );
        };
    };

    template< typename Solver >
    struct Hasher
    {
        using Snapshot = vm::CowHeap::Snapshot;
        using Pool = vm::CowHeap::Pool;

        Pool &_pool;
        Solver &_solver;
        mutable vm::CowHeap _h1, _h2;
        vm::HeapPointer _root;
        bool overwrite = false;

        Hasher( Pool &pool, const vm::CowHeap &heap, Solver &solver )
            : _pool( pool ), _solver( solver ), _h1( heap ), _h2( heap )
        {}

        Hasher( const Hasher &o, Pool &p, Solver &s )
            : Hasher( p, o._h1, s )
        {
            _root = o._root;
            overwrite = o.overwrite;
        }

        void prepare( Snapshot ) {}

        bool equal_fastpath( Snapshot a, Snapshot b ) const
        {
            bool rv = false;
            if ( _pool.size( a ) == _pool.size( b ) )
                rv = std::equal( _h1.snap_begin( _pool, a ), _h1.snap_end( _pool, a ),
                                 _h1.snap_begin( _pool, b ) );
            if ( !rv )
                _h1.restore( _pool, a ), _h2.restore( _pool, b );
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

            PairExtract extract;

            if ( mem::compare( _h1, _h2, _root, _root, extract ) != 0 )
                return false;

            if ( extract.pairs.empty() )
                return true;

            return _solver.equal( extract.pairs, _h1, _h2 );
        }

        auto hash( Snapshot s ) const
        {
            _h1.restore( _pool, s );
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
        using Super = impl::Hasher< Solver >;
        using SPool = brick::mem::SlavePool< typename Super::Pool >;
        mutable SPool _sym_next;

        void prepare( Snapshot s )
        {
            _sym_next.materialise( s, sizeof( Snapshot ) );
        }

        Hasher( typename Super::Pool &pool, const vm::CowHeap &heap, Solver &solver )
            : Super( pool, heap, solver ), _sym_next( pool )
        {}

        Hasher( const Hasher &o, typename Super::Pool &pool, Solver &solver )
            : Super( o, pool, solver ), _sym_next( o._sym_next )
        {}

        template< typename Cell >
        bool match( Cell &cell, Snapshot b, mem::hash64_t h ) const
        {
            auto a = cell.fetch();

            using ASnap = std::atomic< Snapshot >;
            ASnap *a_ptr = nullptr;

            if ( this->equal_fastpath( a, b ) )
                return true;

            while ( true )
            {
                impl::PairExtract extract;

                if ( mem::compare( this->_h1, this->_h2, this->_root, this->_root, extract ) != 0 )
                    return false;

                if ( this->_solver.equal( extract.pairs, this->_h1, this->_h2 ) )
                {
                    if ( this->overwrite )
                    {
                        if ( a_ptr )
                            a_ptr->store( b );
                        else
                            cell.store( b, h );
                    }

                    return true;
                }

                a_ptr = _sym_next.template machinePointer< ASnap >( a );
                a = a_ptr->load();
                if ( this->_pool.valid( a ) )
                    this->_h1.restore( this->_pool, a );
                else
                    break;
            }

            ASSERT( a_ptr );
            a_ptr->store( b );
            return true;
        }
    };

    template<>
    struct Hasher< smt::NoSolver > : impl::Hasher< smt::NoSolver >
    {
        using impl::Hasher< smt::NoSolver >::Hasher;

        template< typename Cell >
        bool match( Cell &a, Snapshot b, mem::hash64_t h ) const
        {
            if ( !this->equal_explicit( a.fetch(), b ) )
                return false;

            if ( this->overwrite )
                a.store( b, h );

            return true;
        }
    };

}

// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Petr Ročkai <code@fixp.eu>
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

#include <divine/smt/extract.hpp>

namespace divine::smt
{

template< typename Builder >
typename Builder::Node Extract< Builder >::build( vm::HeapPointer p )
{
    auto f = read( p );

    if ( isUnary( f->op() ) )
    {
        auto arg = convert( f->unary.child );
        return this->unary( f->unary, arg );
    }

    if ( isBinary( f->op() ) )
    {
        auto a = convert( f->binary.left ), b = convert( f->binary.right );
        return this->binary( f->binary, a, b );
    }

    switch ( f->op() )
    {
        case sym::Op::Variable:
            return this->variable( f->var.type, f->var.id );
        case sym::Op::Constant:
            switch ( f->con.type.type() ) {
                case sym::Type::Int:
                    return this->constant( f->con.type, f->con.value );
                case sym::Type::Float:
                    return this->constant( f->con.type, static_cast< double >( f->con.value ) );
            }
        default: UNREACHABLE_F( "unexpected operation %d", f->op() );
    }
}

template struct Extract< builder::SMTLib2 >;

#if OPT_Z3
template struct Extract< builder::Z3 >;
#endif

#if OPT_STP
template struct Extract< builder::STP >;
#endif

}

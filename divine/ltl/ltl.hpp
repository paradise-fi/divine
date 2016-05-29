/*
 * (c) 2016 Viktória Vozárová <>
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

#include <iostream>
#include "brick-types"

#ifndef LTL2C_LTL_H
#define LTL2C_LTL_H

struct LTL;

using LTLPtr = std::shared_ptr< LTL >;

struct Atom
{
    std::string label;
    std::string string() const { return label; }
    LTLPtr normalForm( bool );
    LTLPtr normalForm();
};

struct Unary
{
    enum Operator { Neg, Global, Future, Next  }  op;
    LTLPtr subExp;

    std::string string() const;
    LTLPtr normalForm( bool );
    LTLPtr normalForm() { return normalForm( false ); }
};

struct Binary
{
    enum Operator { And, Or, Impl, Equiv, Until, Release }  op;
    LTLPtr left, right;

    std::string string() const;
    LTLPtr normalForm( bool );
    LTLPtr normalForm() { return normalForm( false ); }
};

using Exp = brick::types::Union< Atom, Unary, Binary >;
struct LTL : Exp
{
    using Exp::Union;

    struct Comparator
    {
        bool operator()( LTLPtr ltlA, LTLPtr ltlB ) const
        {
            return ltlA->string() == ltlB->string();
        }
    };

    std::string string()
    {
        return apply( [] ( auto e ) -> std::string { return e.string(); } ).value();
    }

    LTLPtr normalForm()
    {
        return normalForm( false );
    }

    LTLPtr normalForm( bool neg )
    {
        return apply( [=] ( auto e ) -> LTLPtr { return e.normalForm( neg ); } ).value();
    }

    static LTLPtr make( Binary::Operator op, LTLPtr left, LTLPtr right )
    {
        Binary binary;
        binary.op = op;
        binary.left = left;
        binary.right = right;
        return std::make_shared< LTL >( binary );
    }

    static LTLPtr make( Unary::Operator op, LTLPtr subExp )
    {
        Unary unary;
        unary.op = op;
        unary.subExp = subExp;
        return std::make_shared< LTL >( unary );
    }

    static LTLPtr make( std::string label )
    {
        Atom atom;
        atom.label = label;
        return std::make_shared< LTL >( atom );
    }

    static LTLPtr make( LTL ltl )
    {
        return std::make_shared< LTL >( ltl );
    }
};

#endif //LTL2C_LTL_H

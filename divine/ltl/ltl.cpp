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

#include "ltl.hpp"

namespace divine {
namespace ltl {

std::string Unary::string() const
{
    switch (op)
    {
        case Neg:
            return "!" + subExp->string();
        case Global:
            return "G" + subExp->string();
        case Future:
            return "F" + subExp->string();
        case Next:
            return "X" + subExp->string();
        default:
            return "";
    }
}

std::string Binary::string() const
{
    switch (op)
    {
        case And:
            return "( " + left->string() + " && " + right->string() + " )";
        case Or:
            return "( " + left->string() + " || " + right->string() + " )";
        case Impl:
            return "( !" + left->string() + " || " + right->string() + " )";
        case Equiv:
            return "( " + left->string() + " == " + right->string() + " )";
        case Until:
            return "( " + left->string() + " U " + right->string() + " )";
        case Release:
            return "( " + left->string() + " R " + right->string() + " )";
        default:
            return "";
    }
}


LTLPtr Atom::normalForm()
{
    return std::make_shared< LTL >( *this );
}

LTLPtr Atom::normalForm( bool neg )
{
    if ( neg )
        return LTL::make( Unary::Neg, normalForm() );
    return normalForm();
}

LTLPtr Unary::normalForm( bool neg )
{
    if ( neg )
    {
        switch ( op )
        {
            case Unary::Neg:
                return subExp->normalForm( !neg );
            // !F(a) = !(true U a) = false R !a
            case Unary::Future:
                return LTL::make(
                        Binary::Release,
                        LTL::make( "false" ),
                        subExp -> normalForm( true ) );
            // !G(a) = !(!F(!a)) = F(!a)
            case Unary::Global:
                return LTL::make(
                        Unary::Future,
                        LTL::make( Unary::Neg, subExp )) -> normalForm();
            // !X(a) = X(!a)
            case Unary::Next:
                return LTL::make(
                        Unary::Next,
                        LTL::make( Unary::Neg, subExp ) -> normalForm( false ));
            default:
                return subExp;
        }
    }
    else
    {
        switch ( op )
        {
            case Unary::Neg:
                return subExp->normalForm( !neg );
            // F(a) = true U a
            case Unary::Future:
                return LTL::make(
                        Binary::Until,
                        LTL::make( "true" ),
                        subExp-> normalForm( false ) );
            // G(a) = false R a
            case Unary::Global:
                return LTL::make(
                        Binary::Release,
                        LTL::make( "false" ),
                        subExp-> normalForm( false ) );
            // X(a) = X(a)
            case Unary::Next:
                return LTL::make( Unary::Next, subExp -> normalForm( false ) );
            default:
                return subExp;
        }
    }
}

LTLPtr Binary::normalForm( bool neg )
{
    if ( neg )
    {
        switch ( op )
        {
            // !(a && b) = (!a || !b)
            case Binary::And:
                return LTL::make(
                        Binary::Or,
                        left -> normalForm( neg ),
                        right -> normalForm( neg ) );
            // !(a || b) = (!a && !b)
            case Binary::Or:
                return LTL::make(
                        Binary::And,
                        left -> normalForm( neg ),
                        right -> normalForm( neg ) );
            // !(a => b) = (a && !b)
            case Binary::Impl:
                return LTL::make(
                        Binary::And,
                        left -> normalForm( !neg ),
                        right -> normalForm( neg ) );
            // !(a <=> b) = ((a && !b) || (!a && b))
            case Binary::Equiv:
            {
                auto first = LTL::make(
                        Binary::And,
                        left -> normalForm( !neg ),
                        right -> normalForm( neg ) );
                auto second = LTL::make(
                        Binary::And,
                        left -> normalForm( neg ),
                        right -> normalForm( !neg ) );
                return LTL::make( Binary::Or, first, second );
            }
            // !(a U b) = (!a R !b)
            case Binary::Until:
                return LTL::make(
                        Binary::Release,
                        left -> normalForm( neg ),
                        right -> normalForm( neg ) );
            // !(a R b) = (!a U !b)
            case Binary::Release:
                return LTL::make(
                        Binary::Until,
                        left -> normalForm( neg ),
                        right -> normalForm( neg ) );
            default:
                return left;
        }
    }
    else
    {
        switch ( op )
        {
            // (a && b) = (a && b), (a || b) = (a || b)
            case Binary::And:
            case Binary::Or:
                return LTL::make(
                        op,
                        left -> normalForm( neg ),
                        right -> normalForm( neg ) );
            // (a => b) = (!a || b)
            case Binary::Impl:
                return LTL::make(
                        Binary::Or,
                        left -> normalForm( !neg ),
                        right -> normalForm( neg ) );
            // (a <=> b) = ((a && b) || (!a && !b))
            case Binary::Equiv:
            {
                auto first = LTL::make(
                        Binary::And,
                        left -> normalForm( neg ),
                        right -> normalForm( neg ) );
                auto second = LTL::make(
                        Binary::And,
                        left -> normalForm( !neg ),
                        right -> normalForm( !neg ) );
                return LTL::make(
                        Binary::Or,
                        first,
                        second );
            }
            // (a U b) = (a U b)
            case Binary::Until:
                return LTL::make(
                        Binary::Until,
                        left -> normalForm( neg ),
                        right -> normalForm( neg ) );
            // (a R b) = (a R b)
            case Binary::Release:
                return LTL::make(
                        Binary::Release,
                        left -> normalForm( neg ),
                        right -> normalForm( neg ) );
            default:
                return left;
        }
    }
}

}
}

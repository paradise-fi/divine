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
#include <map>
#include <list>
#include <cassert>

namespace divine {
namespace ltl {

int LTL::idCounter = 0;

bool helper( LTLPtr ltlA, LTLPtr ltlB )
{
    if( !ltlA || !ltlB ) {
        if( ltlB ) // if A is nullptr but B is defined.
            return true;
        return false;
    }

    if( ltlA->id == ltlB->id )
        return false;
    if( ltlA->priority() < ltlB->priority() )
        return true;
    if( ltlB->priority() < ltlA->priority() )
        return false;
    // ltlA and ltlB are the same type and have the same op
    if( ltlA->is< Boolean >() ) {
        if( ltlA->get< Boolean >().value == ltlB->get< Boolean >().value )
            return ltlA->indexesOfUParents() < ltlB->indexesOfUParents();
        return ltlA->get< Boolean >().value < ltlB->get< Boolean >().value;
    }
    else if( ltlA->is< Atom >() ){
        if( ltlA->string() == ltlB->string() )
            return ltlA->indexesOfUParents() < ltlB->indexesOfUParents();
        return ltlA->string() < ltlB->string();
    }
    if( ltlA->is< Unary >() ) {
        if( helper( ltlA->get< Unary >().subExp, ltlB->get< Unary >().subExp ) )
            return true;
        if( helper( ltlB->get< Unary >().subExp, ltlA->get< Unary >().subExp ) )
            return false;
        return ltlA->indexesOfUParents() < ltlB->indexesOfUParents();
    }
    else { // ltlA->is< Binary >() )
        if( helper( ltlA->get< Binary >().right, ltlB->get< Binary >().right ) )
            return true;
        if( helper( ltlB->get< Binary >().right, ltlA->get< Binary >().right ) )
            return false;
        if( helper( ltlA->get< Binary >().left, ltlB->get< Binary >().left ) )
            return true;
        if( helper( ltlB->get< Binary >().left, ltlA->get< Binary >().left ) )
            return false;
        return ltlA->indexesOfUParents() < ltlB->indexesOfUParents();
    }
}

bool LTLComparator::operator()( LTLPtr ltlA, LTLPtr ltlB ) const
{
    return helper( ltlA, ltlB );
}

bool LTLComparator2::operator()( LTLPtr ltlA, LTLPtr ltlB ) const // without comparing the sets of Until parents
{
    if( !ltlA || !ltlB ) {
        if( ltlB ) // if A is nullptr but B is defined.
            return true;
        return false;
    }
    if( ltlA->id == ltlB->id )
        return false;
    if( ltlA->priority() < ltlB->priority() )
        return true;
    if( ltlB->priority() < ltlA->priority() )
        return false;
    if( ltlA->is< Boolean >() )
        return ltlA->get< Boolean >().value < ltlB->get< Boolean >().value;
    if( ltlA->is< Atom >() )
        return ltlA->string() < ltlB->string();
    if( ltlA->is< Unary >() ) {
        if( operator()( ltlA->get< Unary >().subExp, ltlB->get< Unary >().subExp ) )
            return true;
        return false;
    }
    if( operator()( ltlA->get< Binary >().right, ltlB->get< Binary >().right ) )
        return true;
    if( operator()( ltlB->get< Binary >().right, ltlA->get< Binary >().right ) )
        return false;
    if( operator()( ltlA->get< Binary >().left, ltlB->get< Binary >().left ) )
        return true;
    return false;
}

std::string Unary::string() const
{
    std::string sub = subExp ? subExp->string() : "_";
    switch (op)
    {
        case Neg:
            return "!" + sub;
        case Global:
            return "G" + sub;
        case Future:
            return "F" + sub;
        case Next:
            return "X" + sub;
        default:
            return "";
    }
}

std::string Binary::string() const
{
    std::string l = left ? left->string() : "_";
    std::string r = right ? right->string() : "_";

    switch (op)
    {
        case And:
            return "( " + l + " & " + r + " )";
        case Or:
            return "( " + l + " | " + r + " )";
        case Impl:
            return "( " + l + " -> " + r + " )";
        case Equiv:
            return "( " + l + " <-> " + r + " )";
        case Until:
            return "( " + l + " U " + r + " )";
        case Release:
            return "( " + l + " R " + r + " )";
        default:
            return "";
    }
}


LTLPtr Atom::normalForm()
{
    assert( subExp && "formula should have been already complete" );
    return subExp->countAndLabelU( seed );
}

//returns the count of different visited U, postorder
int Binary::countAndLabelU( int seed  /* = 0 */ )
{
    assert( left && right && "formula should have been already complete" );
    int l = left->countAndLabelU( seed );
    assert( l >= seed );
    int r = right->countAndLabelU( l );
    assert( r >= l );
    if( op != Until || untilIndex != -1 )
        return r;
    untilIndex = r;
    return r + 1;
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

/**
 * ******************************
 * ******** PARSE TOOLS *********
 * ******************************
 */

struct BracketOpen  {};
struct BracketClose {};

struct Tokens;
using Token = brick::types::Union< LTLPtr, BracketOpen, BracketClose, std::shared_ptr< Tokens > >;
struct Tokens : std::list< Token > {};
using TokensPtr = std::shared_ptr< Tokens >;
using Operator = brick::types::Union< Unary::Operator, Binary::Operator >;

std::vector< std::pair< Operator, bool > > operatorOrder = {
    { Unary::Neg,    false },
    { Unary::Global, false }, {Unary::Future,   false},  {Unary::Next, false},
    { Binary::And,   false }, {Binary::Or,      false},
    { Binary::Equiv, false },
    { Binary::Impl,  false },
    { Binary::Until, true },  {Binary::Release, false}
};

std::map< std::string, Operator > stringsToOperators = {
    { "!", Unary::Neg    },
    { "G", Unary::Global }, { "F", Unary::Future  }, { "X", Unary::Next },
    { "&", Binary::And   }, { "|", Binary::Or     },
    { "=", Binary::Equiv },
    { "->", Binary::Impl },
    { "U", Binary::Until }, { "R", Binary::Release }
};

Tokens tokenizer( const std::string& formula )
{
    Tokens tokens;

    for ( auto it = formula.begin(); it < formula.end(); ++it )
    {
        std::string proposition;
        while ( it != formula.end() && *it >= 'a' && *it <= 'z' )
            proposition.push_back( *it++ );
        if ( !proposition.empty() )
            tokens.push_back( LTL::make(proposition) );
        if ( it != formula.end() )
        {
            if( *it == ' ' )
                continue;
            else if( *it == '-' )
            {
                if( it + 1 != formula.end() && *(it + 1) == '>' )
                    tokens.push_back( LTL::make( Binary::Impl ) ), ++it;
                else throw std::invalid_argument( "invalid LTL formula, cannot resolve symbol '-' unless '>' follows" );
            }
            else if ( *it == '(' )
                tokens.push_back( BracketOpen() );
            else if ( *it == ')' )
                tokens.push_back( BracketClose() );
            else
            {
                auto search = stringsToOperators.find( std::string(1, *it) );
                if ( search != stringsToOperators.end() )
                    search->second.match(
                        [&]( Unary::Operator op ) { tokens.push_back( LTL::make( op ) ); },
                        [&]( Binary::Operator op ) { tokens.push_back( LTL::make( op ) ); } );
                else
                    throw std::invalid_argument( std::string( "invalid LTL formula, cannot resolve symbol " ) + *it );
            }
        }
    }
    return tokens;
}

/*
* ****************************************
* *********** SOLVE BRACKETS *************
*/
Tokens::iterator brackets( TokensPtr root, Tokens::iterator it, Tokens::iterator end )
{
    while ( it != end ) {
        if ( it->is< BracketOpen >() )
        {
            auto newNode = std::make_shared<Tokens>();
            it = brackets( newNode, std::next(it, 1), end );
            if( it == end )
                throw std::invalid_argument( "Formula syntax error - too many brackets '('" );
            if(newNode->empty())
                throw std::invalid_argument( "Formula syntax error - empty brackets () are not allowed" );
            root->push_back( newNode );
        }
        else if ( it->is< BracketClose >() )
            return it;
        else
            root->push_back( *it );
        it++;
    }
    return it;
}

TokensPtr solveBrackets( const std::string& formula )
{
    auto formulaNode = std::make_shared< Tokens >( tokenizer( formula ) );
    auto newTree = std::make_shared< Tokens >();
    if ( brackets( newTree, formulaNode->begin(), formulaNode->end() ) != formulaNode->end() )
        throw std::invalid_argument( "Formula syntax error - too many brackets ')'" );
    return newTree;
}

/*
* ****************************************
* ************** COLLAPSE ****************
*/

bool reduceUnary( TokensPtr leaf, Unary::Operator op_type )
{
    bool changed = false;
    if ( leaf->size() < 2 )
        return false;
    Tokens::iterator it = leaf->begin();

    while ( it != leaf->end() )
    {
        ASSERT( it->is< LTLPtr >() && "Leaf should contain LTL only" );
        auto op = it->get< LTLPtr >();
        if( op->is<Unary>() && op->get< Unary >().op == op_type && !op->isComplete() )
        {
            if( std::next( it ) == leaf->end() )
                throw std::invalid_argument( "LTL formula must not end with " + op->string() );
            auto operand = std::next( it )->get< LTLPtr >();
            if( operand->isComplete() )
            {
                *it = LTL::make( op_type, operand );
                leaf->erase(std::next( it ));
                changed = true;
            }
        }
        ++it;
    }
    return changed;
}

bool reduceBinary( TokensPtr leaf, Binary::Operator op_type, bool reverse )
{
    auto first = reverse ? std::prev( leaf->end() ) : leaf->begin();
    auto last  = reverse ? leaf->begin() : std::prev( leaf->end() );

    auto it = first;
    bool changed = false;

    while ( leaf->size() > 2 )
    {
        bool done = ( reverse ? std::prev( it ) : std::next( it ) ) == last;
        ASSERT( it->is< LTLPtr >() && "Leaf should contain LTL only" );
        auto op = it->get< LTLPtr >();
        if( op->is< Binary >() && op->get< Binary >().op == op_type && !op->isComplete() )
        {
            if( std::next( it ) == leaf->end() )
                throw std::invalid_argument( "LTL formula must not end with " + op->string() );
            if( it == leaf->begin() )
                throw std::invalid_argument( "LTL formula must not begin with " + op->string() );

            auto left = std::prev( it )->get< LTLPtr >();
            auto right = std::next( it )->get< LTLPtr >();

            if( left->isComplete() && right->isComplete() )
            {
                LTLPtr x;
                *it = x = LTL::make( op_type, left, right );
                leaf->erase( std::prev( it ) );
                leaf->erase( std::next( it ) );
            }
        }
        if ( done ) return changed;
        if ( reverse ) --it; else ++it;
    }
    return changed;
}

LTLPtr reduce( TokensPtr leaf )
{
    while ( leaf->size() > 1 )
    {
        auto size = leaf->size();
        for ( auto pair : operatorOrder )
        {
            auto unary = [&]( Unary::Operator op ) { return reduceUnary( leaf, op ); };
            auto binary = [&]( Binary::Operator op ) { return reduceBinary( leaf, op, pair.second ); };
            if ( pair.first.match( unary, binary ).value() )
                break;
        }
        if( leaf->size() == size )
            throw std::invalid_argument( "Syntax error in LTL formula");
    }
    ASSERT( !leaf->empty() );
    return leaf->front().get< LTLPtr >();
}

LTLPtr collapse( TokensPtr root )
{
    if ( !root )
        throw std::runtime_error( "Called collapse on nullptr root" );
    for ( auto& child : *root )
        child.apply( [&]( TokensPtr n ) { child = collapse( n ); } );
    return reduce( root );
}

LTLPtr LTL::parse( const std::string& str )
{
    return collapse( solveBrackets( str ) );
}

}
}

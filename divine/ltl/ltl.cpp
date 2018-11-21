/*
 * (c) 2016 Viktória Vozárová <vikiv@mail.muni.cz>
 * (c) 2017 Tadeáš Kučera <kucerat@mail.muni.cz>
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

//defines ordering on LTLs
bool ord( LTLPtr ltlA, LTLPtr ltlB ) // also comparing the sets of Until parents
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
        if( ord( ltlA->get< Unary >().subExp, ltlB->get< Unary >().subExp ) )
            return true;
        if( ord( ltlB->get< Unary >().subExp, ltlA->get< Unary >().subExp ) )
            return false;
        return ltlA->indexesOfUParents() < ltlB->indexesOfUParents();
    }
    else { // ltlA->is< Binary >() )
        if( ord( ltlA->get< Binary >().right, ltlB->get< Binary >().right ) )
            return true;
        if( ord( ltlB->get< Binary >().right, ltlA->get< Binary >().right ) )
            return false;
        if( ord( ltlA->get< Binary >().left, ltlB->get< Binary >().left ) )
            return true;
        if( ord( ltlB->get< Binary >().left, ltlA->get< Binary >().left ) )
            return false;
        return ltlA->indexesOfUParents() < ltlB->indexesOfUParents();
    }
}

bool LTLComparator::operator()( LTLPtr ltlA, LTLPtr ltlB ) const // also comparing the sets of Until parents
{
    return ord( ltlA, ltlB );
}
bool ord2( LTLPtr ltlA, LTLPtr ltlB ) // without comparing the sets of Until parents
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
        if( ord2( ltlA->get< Unary >().subExp, ltlB->get< Unary >().subExp ) )
            return true;
        return false;
    }
    if( ord2( ltlA->get< Binary >().right, ltlB->get< Binary >().right ) )
        return true;
    if( ord2( ltlB->get< Binary >().right, ltlA->get< Binary >().right ) )
        return false;
    if( ord2( ltlA->get< Binary >().left, ltlB->get< Binary >().left ) )
        return true;
    return false;
}
bool equal2( LTLPtr ltlA, LTLPtr ltlB ) //without comparing the sets of Until parents
{
    return !ord2( ltlA, ltlB ) && !ord2( ltlB, ltlA );
}

bool LTLComparator2::operator()( LTLPtr ltlA, LTLPtr ltlB ) const // without comparing the sets of Until parents
{
    return ord2( ltlA, ltlB );
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

int Unary::countAndLabelU( int seed /* = 0 */ )
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

LTLPtr Boolean::normalForm() { return LTL::make( value ); } // returns shared pointer to a NEW COPY of the Bool

LTLPtr Boolean::normalForm( bool neg )
{
    if ( neg )
        return LTL::make( !value );
    return normalForm();
}

LTLPtr Atom::normalForm() { return LTL::make( label ); } // returns shared pointer to a NEW COPY of the Atom

LTLPtr Atom::normalForm( bool neg )
{
    if ( neg )
        return LTL::make( Unary::Neg, normalForm() );
    return normalForm();
}

LTLPtr Unary::normalForm( bool neg )
{
    if( !subExp )
        throw std::invalid_argument("invalid LTL formula: unary operation has no argument");
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
                        LTL::make( false ),
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
                        LTL::make( true ),
                        subExp->normalForm( false ) );
            // G(a) = false R a
            case Unary::Global:
                return LTL::make(
                        Binary::Release,
                        LTL::make( false ),
                        subExp->normalForm( false ) );
            // X(a) = X(a)
            case Unary::Next:
                return LTL::make( Unary::Next, subExp->normalForm( false ) );
            default:
                return subExp;
        }
    }
}

LTLPtr Binary::normalForm( bool neg )
{
    if( !left )
        throw std::invalid_argument("invalid LTL formula: binary operation has no left argument");
    if( !right )
        throw std::invalid_argument("invalid LTL formula: binary operation has no right argument");
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
            //!(a W b) = !b U ( !a && !b )
            case Binary::WeakUntil:
            {
                auto arg = LTL::make(
                        Binary::And,
                        left -> normalForm( neg ),
                        right -> normalForm( neg ) );
                return LTL::make(
                        Binary::Until,
                        right -> normalForm( neg ),
                        arg );
            }
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
            // (a W b) = b R (a || b)
            case Binary::WeakUntil:
            {
                auto arg = LTL::make(
                        Binary::Or,
                        left->normalForm( neg ),
                        right->normalForm( neg ) );
                return LTL::make(
                        Binary::Release,
                        right->normalForm( neg ),
                        arg );
            }
            default:
                return left;
        }
    }
}

/**
 * ******************************
 * ******** PARSE TOOLS *******
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
    { Binary::Until, true }, { Binary::WeakUntil, false },  {Binary::Release, false}
};

std::map< std::string, Operator > stringsToOperators = {
    { "!", Unary::Neg    },
    { "G", Unary::Global }, { "[]", Unary::Global }, { "F", Unary::Future  }, { "<>", Unary::Future  }, { "X", Unary::Next },
    { "&&", Binary::And   }, { "/\\", Binary::And   }, { "\\/", Binary::Or     }, { "||", Binary::Or     },
    { "=", Binary::Equiv }, { "<->", Binary::Equiv },
    { "->", Binary::Impl },
    { "U", Binary::Until }, { "W", Binary::WeakUntil }, { "V", Binary::Release }, { "R", Binary::Release }
};


Tokens tokenizerRERS( const std::string& formula )
{
    Tokens tokens;

    for ( auto it = formula.begin(); it < formula.end(); ++it )
    {
        std::string proposition;
        if( it != formula.end() && !( *it == 'G' || *it == 'F' || *it == 'X' || *it == 'W' || *it == 'U' || *it == 'V' || *it == 'R' ) )
            while ( it != formula.end() && ( ( *it >= 'A' && *it <= 'Z' ) || ( *it >= 'a' && *it <= 'z' ) || *it == '_' || ( *it >= '0' && *it <= '9' ) ) )
                proposition.push_back( *it++ );
        if ( !proposition.empty() )
            tokens.push_back( LTL::make(proposition) );
        if ( it != formula.end() )
        {
            if( *it == ' ' )
                continue;
            else if( *it == '<' )
            {
                if( it + 1 != formula.end() && *( it + 1 ) == '-' && it + 2 != formula.end() && *(it + 2) == '>' )
                    tokens.push_back( LTL::make( Binary::Equiv ) ), it = it + 2;
                else if( it + 1 != formula.end() && *(it + 1) == '>' )
                    tokens.push_back( LTL::make( Unary::Future ) ), ++it;
                else throw std::invalid_argument( "invalid LTL formula: cannot resolve symbol '<' unless '>' or '->' follows" );
            }
            else if ( *it == '(' )
                tokens.push_back( BracketOpen() );
            else if ( *it == ')' )
                tokens.push_back( BracketClose() );
            else if( *it == '&' ) {
                if( it + 1 == formula.end() )
                    throw std::invalid_argument("invalid LTL formula: symbol missing after &" );
                tokens.push_back( LTL::make( Binary::And ) );
                if( *(it + 1) == '&' )
                    ++it;
            }
            else if( *it == '|' ) {
                if( it + 1 == formula.end() )
                    throw std::invalid_argument("invalid LTL formula: symbol missing after |" );
                tokens.push_back( LTL::make( Binary::Or ) );
                if( *(it + 1) == '|' )
                    ++it;
            }
            else
            {
                auto search = stringsToOperators.find( std::string(1, *it) ); //looking for 1 character
                if ( search != stringsToOperators.end() )
                {
                    search->second.match(
                        [&]( Unary::Operator op ) { tokens.push_back( LTL::make( op ) ); },
                        [&]( Binary::Operator op ) { tokens.push_back( LTL::make( op ) ); } );
                }
                else
                {
                    std::string tmp = std::string(1, *it);
                    if( ++it == formula.end() )
                        throw std::invalid_argument("invalid LTL formula: some symbol missing at the end");
                    tmp += *it;
                    search = stringsToOperators.find( tmp );
                    if( search != stringsToOperators.end() )
                        search->second.match(
                            [&]( Unary::Operator op ) { tokens.push_back( LTL::make( op ) ); },
                            [&]( Binary::Operator op ) { tokens.push_back( LTL::make( op ) ); } );
                    else
                        throw std::invalid_argument("invalid LTL formula");
                }
            }
        }
    }
    return tokens;
}

// propositions must be in lowercase, with numbers
Tokens tokenizer( const std::string& formula, bool typeRERS /*= false*/ )
{
    if( typeRERS )
        return tokenizerRERS( formula );
    Tokens tokens;
    for ( auto it = formula.begin(); it < formula.end(); ++it )
    {
        std::string proposition;
        while ( it != formula.end() && ( ( *it >= 'a' && *it <= 'z' ) || *it == '_' || ( *it >= '0' && *it <= '9' ) ) )
            proposition.push_back( *it++ );
        if ( !proposition.empty() )
            tokens.push_back( LTL::make(proposition) );
        if ( it != formula.end() )
        {
            if( *it == ' ' )
                continue;
            else if( *it == '<' )
            {
                if( it + 1 != formula.end() && *( it + 1 ) == '-' && it + 2 != formula.end() && *(it + 2) == '>' )
                    tokens.push_back( LTL::make( Binary::Equiv ) ), it = it + 2;
                else if( it + 1 != formula.end() && *(it + 1) == '>' )
                    tokens.push_back( LTL::make( Unary::Future ) ), ++it;
                else throw std::invalid_argument( "invalid LTL formula, cannot resolve symbol '<' unless '>' or '->' follows" );
            }
            else if ( *it == '(' )
                tokens.push_back( BracketOpen() );
            else if ( *it == ')' )
                tokens.push_back( BracketClose() );
            else if( *it == '&' ) {
                if( it + 1 == formula.end() )
                    throw std::invalid_argument("invalid LTL formula: symbol missing after &" );
                tokens.push_back( LTL::make( Binary::And ) );
                if( *(it + 1) == '&' )
                    ++it;
            }
            else if( *it == '|' ) {
                if( it + 1 == formula.end() )
                    throw std::invalid_argument("invalid LTL formula: symbol missing after |" );
                tokens.push_back( LTL::make( Binary::Or ) );
                if( *(it + 1) == '|' )
                    ++it;
            }
            else
            {
                auto search = stringsToOperators.find( std::string(1, *it) );
                if ( search != stringsToOperators.end() )
                {
                    search->second.match(
                        [&]( Unary::Operator op ) { tokens.push_back( LTL::make( op ) ); },
                        [&]( Binary::Operator op ) { tokens.push_back( LTL::make( op ) ); } );
                }
                else
                {
                    std::string tmp = std::string(1, *it);
                    if( ++it == formula.end() )
                        throw std::invalid_argument("invalid LTL formula: some symbol missing at the end");
                    tmp += *( it );
                    search = stringsToOperators.find( tmp );
                    if( search != stringsToOperators.end() )
                        search->second.match(
                            [&]( Unary::Operator op ) { tokens.push_back( LTL::make( op ) ); },
                            [&]( Binary::Operator op ) { tokens.push_back( LTL::make( op ) ); } );
                    else
                        throw std::invalid_argument("invalid LTL formula");
                }
            }
        }
    }
    return tokens;
}

/*
* ****************************************
* *********** SOLVE BRACKETS **********
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

TokensPtr solveBrackets( const std::string& formula, bool typeRERS )
{
    auto formulaNode = std::make_shared< Tokens >( tokenizer( formula, typeRERS /*= false*/ ) );
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

LTLPtr LTL::parse( const std::string& str, bool typeRERS /*= false*/ )
{
    return collapse( solveBrackets( str, typeRERS ) );
}

/**
 * ******************************
 * ********* SIMPLIFY ***********
 * ******** the formula *********
 * ******************************
 */


LTLPtr LTL::Simplifier::makeSimpler( LTLPtr form )
{
    if( form->isAtomOrBooleanOrNeg() )                                      // a, !a, True, False
        return form;
    if( form->is< Unary >() )
    {
        auto op = form->get< Unary >().op;
        auto subExp = form->get< Unary >().subExp;
        if( op == Unary::Global )                                                    // G _
        {
            // G G x = G x
            if( subExp->isType( Unary::Global ) )
            {
                auto subSubExp = subExp->get<Unary>().subExp;
                changed = true;
                return LTL::make( Unary::Global, makeSimpler( subSubExp ) );
            }
            // G( p  && GFq ) = (Gp) || (GFq)
            else if( subExp->isType( Binary::Or ) ) {                                    // G( _||_ )
                auto r = subExp->get<Binary>().right;
                auto l = subExp->get<Binary>().left;
                if( r->isType( Unary::Global ) ) {                                      // G( _|| G_ )
                    auto rsub = r->get< Unary >().subExp;
                    if( rsub->isType( Unary::Future ) ) {                                 // G(_|| GF_  )
                        auto q = rsub->get< Unary >().subExp;
                        auto newGFq = LTL::make( Unary::Global, LTL::make( Unary::Future, makeSimpler( q ) ) );
                        changed = true;
                        return LTL::make( Binary::Or, LTL::make( Unary::Global, makeSimpler( l ) ), newGFq );
                    }
                }
                if( l->isType( Unary::Global ) ) {                                      // G( G_|| _ )
                    auto lsub = l->get< Unary >().subExp;
                    if( lsub->isType( Unary::Future ) ) {                                 // G( GF_|| _  )
                        auto q = lsub->get< Unary >().subExp;
                        auto newGFq = LTL::make( Unary::Global, LTL::make( Unary::Future, makeSimpler( q ) ) );
                        changed = true;
                        return LTL::make( Binary::Or, LTL::make( Unary::Global, makeSimpler( r ) ), newGFq );
                    }
                }
            }
            return LTL::make( Unary::Global, makeSimpler( subExp ) );
        }
        else if( op == Unary::Future )
        {
            // F G F x = G F x
            if( subExp->isType( Unary::Global ) )                             // F G _
            {
                auto subSubExp = subExp->get<Unary>().subExp;
                if( subSubExp->isType( Unary::Future ) )  // F G F _
                {
                    changed = true;
                    return makeSimpler( subExp );
                }
            }
            // F( p  && GFq ) = (Fp) && (GFq)
            else if( subExp->isType( Binary::And) )
            {
                auto r = subExp->get<Binary>().right;
                auto l = subExp->get<Binary>().left;
                if( r->isType( Unary::Global ) )                                      // F( _&& G_ )
                {
                    auto rsub = r->get< Unary >().subExp;
                    if( rsub->isType( Unary::Future ) )                             // F(_&& GF_  )
                    {
                        auto q = rsub->get< Unary >().subExp;
                        auto newGFq = LTL::make( Unary::Global, LTL::make( Unary::Future, makeSimpler( q ) ) );
                        changed = true;
                        return LTL::make( Binary::And, LTL::make( Unary::Future, makeSimpler( l ) ), newGFq );
                    }
                }
                if( l->isType( Unary::Global ) )                                      // F( G_&& _ )
                {
                    auto lsub = l->get< Unary >().subExp;
                    if( lsub->isType( Unary::Future ) )                                // F( GF_&& _  )
                    {
                        auto q = lsub->get< Unary >().subExp;
                        auto newGFq = LTL::make( Unary::Global, LTL::make( Unary::Future, makeSimpler( q ) ) );
                        changed = true;
                        return LTL::make( Binary::And, LTL::make( Unary::Future, makeSimpler( r ) ), newGFq );
                    }
                }
            }
        }
        return LTL::make( op, makeSimpler( subExp ) );
    }
    else
    {
        auto op = form->get< Binary >().op;
        auto right = form->get< Binary >().right;
        auto left = form->get< Binary >().left;
        if( op == Binary::Until )                               // p U false = false   \/   p U true = true
        {
            if( right->is<Boolean>() )
            {
                changed = true;
                return right;
            }
        }
        else if( op == Binary::And )
        {
            if( equal2( right, left ) )
            {
                changed = true;
                return makeSimpler( right );
            }
            else if( right->is< Boolean >() )
            {
                changed = true;
                if( right->get< Boolean >().value )
                    return makeSimpler( left );
                return right;
            }
            else if( left->is<Boolean>() )
            {
                changed = true;
                if( left->get< Boolean >().value )
                    return makeSimpler( right );
                return left;
            }
            else if( right->isType( Unary::Neg ) )
            {
                if( equal2( left, right->get< Unary >().subExp) )
                {
                    changed = true;
                    return LTL::make( false );
                }
            }
            else if( left->isType( Unary::Neg ) )
            {
                if( equal2( right, left->get< Unary >().subExp) )
                {
                    changed = true;
                    return LTL::make( false );
                }
            }
            else if( left->isType( Binary::Release ) && right->isType( Binary::Release ) )
            {
                auto leftL = left->get< Binary >().left;
                auto rightL = right->get< Binary >().left;
                if( equal2( leftL, rightL ) )
                {
                    auto leftR = left->get< Binary >().right;
                    auto rightR = right->get< Binary >().right;
                    changed = true;
                    auto conjunct = LTL::make( Binary::And, makeSimpler( leftR ), makeSimpler( rightR ) );
                    return LTL::make( Binary::Release, makeSimpler( leftL ), conjunct );
                }
            }
        }
        else if( op == Binary::Or )
        {
            if( equal2( right, left ) )
            {
                changed = true;
                return makeSimpler( right );
            }
            else if( right->is< Boolean >() )
            {
                changed = true;
                if( !right->get< Boolean >().value )
                    return makeSimpler( left );
                return right;
            }
            else if( left->is<Boolean>() )
            {
                changed = true;
                if( !left->get< Boolean >().value )
                    return makeSimpler( right );
                return left;
            }
            else if( right->isType( Unary::Neg ) )
            {
                if( equal2( left, right->get< Unary >().subExp) )
                {
                    changed = true;
                    return LTL::make( true );
                }
            }
            else if( left->isType( Unary::Neg ) ) {
                if( equal2( right, left->get< Unary >().subExp) )
                {
                    changed = true;
                    return LTL::make( true );
                }
            }
            else if( left->isType( Binary::Release ) && right->isType( Binary::Release ) )
            {
                auto leftR = left->get< Binary >().right;
                auto rightR = right->get< Binary >().right;
                if( equal2( leftR, rightR ) )
                {
                    auto leftL = left->get< Binary >().left;
                    auto rightL = right->get< Binary >().left;
                    changed = true;
                    auto disjunct = LTL::make( Binary::Or, makeSimpler( leftL ), makeSimpler( rightL ) );
                    return LTL::make( Binary::Release, disjunct, makeSimpler( leftR ) );
                }
            }
            else if( left->isType( Unary::Global ) && right->isType( Unary::Global ) )
            {
                auto leftSub = left->get< Unary >().subExp;
                auto rightSub = right->get< Unary >().subExp;
                if( leftSub->isType( Unary::Future ) && rightSub->isType( Unary::Future ) )
                {
                    auto leftSubSub = leftSub->get< Unary >().subExp;
                    auto rightSubSub = rightSub->get< Unary >().subExp;
                    changed = true;
                    auto disjunct = LTL::make( Binary::Or, makeSimpler( leftSubSub ), makeSimpler( rightSubSub ) );
                    return LTL::make( Unary::Global, LTL::make( Unary::Future, disjunct ) );
                }
            }
        }
        return LTL::make( op, makeSimpler( left ), makeSimpler( right ) );
    }
}

}
}






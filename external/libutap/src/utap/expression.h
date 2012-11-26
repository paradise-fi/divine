// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-

/* libutap - Uppaal Timed Automata Parser.
   Copyright (C) 2002-2006 Uppsala University and Aalborg University.
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA
*/

#ifndef UTAP_EXPRESSION_HH
#define UTAP_EXPRESSION_HH

#include <vector>
#include <set>
#include <map>

#include "utap/common.h"
#include "utap/symbols.h"
#include "utap/position.h"

namespace UTAP
{
    /** 
        A reference to an expression.

        An expression is a tree of operations and is identified by the
        root of the tree. There are many kinds of operations (see kind_t),
        some are leafs in the tree (identifers, constants), some are
        unary operations (unary minus, negation), some are binary (plus,
        minus, etc.), and some are n-ary (lists, function calls).

        Expressions can be annotated with types. The type of an
        expression should never contain information about whether the
        expression is a variable reference, a constant value or about
        the range of the result value. Use the TypeChecker and
        RangeChecker classes to determine these informations.

        All nodes have the following attributes: 

        kind     indicates the kind of operation
        position indicates the position of the expression in the input file.
        type     the type of the result of evaluating the expression
        size     the number of subexpressions
        sub      the subexpressions
        
        Some nodes have extra attributes (depending on the kind):

        value    the value of a CONSTANT node
        sync     the synchronisation of a SYNC node
        index    the index of a DOT node
        symbol   the symbol of an IDENTIFIER node

        Expressions are created by using the static factory methods.
    */

    class expression_t 
    {
    private:
        expression_t(Constants::kind_t, const position_t &);
    public:
        /** Default constructor. Creates an empty expression. */
        expression_t();

        /** Copy constructor. */
        expression_t(const expression_t &);

        /** Destructor. */
        ~expression_t();

        /** Make a shallow clone of the expression. */
        expression_t clone() const;

        /** Returns the kind of the expression. */
        Constants::kind_t getKind() const;

        /** Returns the number of subexpression. */
        size_t getSize() const;

        /** Returns the position of this expression. */
        const position_t &getPosition() const;        
        
        /** Returns the type of the expression. */
        type_t getType() const;

        /** Sets the type of the expression. */
        void setType(type_t);

        /** Returns the value field of this expression. This
            call is not valid for all expressions. */
        int32_t getValue() const;

        /** Returns the index field of this expression. */
        int32_t getIndex() const;

        /** Returns true if this is an empty expression. */
        bool empty() const;

        /** Returns the synchronisation type of SYNC operations. */
        Constants::synchronisation_t getSync() const;

        /** Returns a string representation of the expression. */
        std::string toString(bool old = false) const;

        /** Returns the ith subexpression. */
        expression_t &operator[](uint32_t);
        
        /** Returns the ith subexpression. */
        const expression_t operator[](uint32_t) const;

        /** Returns the ith subexpression. */
        expression_t &get(uint32_t);

        /** Returns the ith subexpression. */
        const expression_t &get(uint32_t) const;

        /** Assignment operator. */
        expression_t &operator=(const expression_t &);

        /** Equality operator */
        bool equal(const expression_t &) const;
        
        /** 
         *  Returns the symbol of a variable reference. The expression
         *  must be a left-hand side value. In case of
         *  dot-expressions, the record/process symbol is returned. In
         *  case of an inline if, the 'true' branch is returned.
         *
         *  (a=1).getSymbol() returns 'a'
         *  (s.f).getSymbol() returns 's'
         *  (i<1?j:k).getSymbol() returns 'j'
         */
        symbol_t getSymbol();

        /** 
         * Returns the set of symbols this expression might resolve
         * into. In case of inline if, both the 'true' and 'false'
         * branch is added. In case of dot-expressions, both the left
         * hand reference and the member field are returned.
         *
         * (a=1).getSymbol() returns 'a'
         * (s.f).getSymbol() returns 's,f'
         * (i<1?j:k).getSymbol() returns 'j,k'
         */
        void getSymbols(std::set<symbol_t> &symbols) const;

        /** Returns the symbol this expression evaluates to. Notice
            that not all expression evaluate to a symbol. */
        const symbol_t getSymbol() const;

        /** Returns true if this expression is a reference to a
            symbol in the given set. */
        bool isReferenceTo(const std::set<symbol_t> &) const;

        /** True if this expression can change any of the variables
            identified by the given symbols. */
        bool changesVariable(const std::set<symbol_t> &) const;

        /** True if this expression can change any variable at all. */
        bool changesAnyVariable() const;

        /** True if the evaluation of this expression depends on 
            any of the symbols in the given set. */
        bool dependsOn(const std::set<symbol_t> &) const;

        void collectPossibleWrites(std::set<symbol_t> &) const;
        void collectPossibleReads(std::set<symbol_t> &) const;

         /** Less-than operator. Makes it possible to put expression_t
             objects into an STL set. */
         bool operator < (const expression_t) const;
 
         /** Equallity operator. Returns true if the two references point
             to the same expression object. */
         bool operator == (const expression_t) const;

        expression_t subst(symbol_t, expression_t) const;
 
         static int getPrecedence(Constants::kind_t); 

        /** Create a CONSTANT expression. */
        static expression_t createConstant(int32_t, position_t = position_t());

        /** Create an IDENTIFIER expression */
        static expression_t createIdentifier(symbol_t, position_t = position_t());

        /** Create a unary expression */
        static expression_t createUnary(Constants::kind_t, expression_t,
                                        position_t = position_t(),
                                        type_t = type_t());
        /** Create a binary expression */
        static expression_t createBinary(Constants::kind_t,
                                         expression_t, expression_t,
                                         position_t = position_t(),
                                         type_t = type_t());

        /** Create a ternary expression */
        static expression_t createTernary(Constants::kind_t, expression_t, 
                                          expression_t, expression_t,
                                          position_t = position_t(),
                                          type_t = type_t());

        /** Create an n-ary expression */
        static expression_t createNary(Constants::kind_t, 
                                       const std::vector<expression_t> &,
                                       position_t = position_t(),
                                       type_t = type_t());

        /** Create a DOT expression */
        static expression_t createDot(expression_t, int32_t = -1,
                                      position_t = position_t(), 
                                      type_t = type_t());

        /** Create a SYNC expression */
        static expression_t createSync(expression_t, 
                                       Constants::synchronisation_t,
                                       position_t = position_t());

        /** Create a DEADLOCK expression */
        static expression_t createDeadlock(position_t = position_t());

    private:
        struct expression_data;
        expression_data *data;
        int getPrecedence() const;
        void toString(bool, char *&str, char *&end, int &size) const;
    };
}

std::ostream &operator<< (std::ostream &o, const UTAP::expression_t &e);

#endif




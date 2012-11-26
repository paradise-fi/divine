// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-

/* libutap - Uppaal Timed Automata Parser.
   Copyright (C) 2002-2003 Uppsala University and Aalborg University.
   
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

#ifndef UTAP_EXPRESSIONBUILDER_HH
#define UTAP_EXPRESSIONBUILDER_HH

#include <stack>
#include <vector>
#include <cassert>

#include "abstractbuilder.h"
#include "utap.h"

namespace UTAP
{
    /** 
     * Partial implementation of the builder interface: The
     * ExpressionBuilder implements all expression related
     * methods. 
     *
     * In order to support quantifier expressions, this class also
     * handles the type related methods.
     *
     * This class does not implement any declaration related methods.
     *
     * Internally, three stacks are maintained: One for expressions,
     * one for types and for frames (scopes).
     */
    class ExpressionBuilder : public AbstractBuilder 
    {
    public:
        class ExpressionFragments 
        {
        private:
            std::vector<expression_t> data;
        public:
            expression_t &operator[] (int idx)
                { return data[data.size() - idx - 1]; }
            void push(expression_t e)
                { data.push_back(e); }
            void pop()
                { data.pop_back(); }
            void pop(uint32_t n);
            uint32_t size() { return data.size(); }
        };

        class TypeFragments 
        {
        private:
            std::vector<type_t> data;
        public:
            type_t &operator[](int idx)
                { return data[data.size() - idx - 1]; }
            void push(type_t value)
                { data.push_back(value); }
            void pop()
                { assert(!data.empty()); data.pop_back(); }
            void duplicate()
                { assert(!data.empty()); data.push_back(data.back()); }
        };

    protected:
        /** Expression stack. */
        ExpressionFragments fragments;

        /** Type stack. */
        TypeFragments typeFragments;

        /** Frame stack. */
        std::stack<frame_t> frames;

        /** Pointer to the system under construction. */
        TimedAutomataSystem *system;

        /** The template currently being parsed. */
        template_t *currentTemplate;

        /** Counter for creating unique scalarset names. */
        int32_t scalar_count;

        /** Push a new frame. */
        void pushFrame(frame_t);
        
        /** Pop the topmost frame. */
        void popFrame();

        bool resolve(std::string, symbol_t &);

        expression_t makeConstant(int value);

        /**
         * Given a prefix and a type, this method creates a new type
         * by applying the prefix. TypeExceptions might be thrown if
         * the combination of the prefix and the type is illegal.
         */
        type_t applyPrefix(PREFIX, type_t type);

        /**
         * If this method returns true, it is allowed to access the
         * private identifiers of a process by prefixing the
         * identifier with the process name.
         *
         * This is only interesting when parsing properties. In this
         * case the method should be overridden by a sub class.
         */
        virtual bool allowProcessReferences() { return false; }

    public:
        ExpressionBuilder(TimedAutomataSystem *);
        ExpressionFragments &getExpressions();

        virtual void addPosition(
            uint32_t position, uint32_t offset, uint32_t line, std::string path);

        virtual void handleError(std::string);
        virtual void handleWarning(std::string);
        virtual void typeDuplicate();
        virtual void typePop();
        virtual void typeBool(PREFIX);
        virtual void typeInt(PREFIX);
        virtual void typeBoundedInt(PREFIX);
        virtual void typeChannel(PREFIX);
        virtual void typeClock();
        virtual void typeVoid();
        virtual void typeScalar(PREFIX);
        virtual void typeName(PREFIX, const char* name);
        virtual bool isType(const char*);
        virtual void exprTrue();
        virtual void exprFalse();
        virtual void exprId(const char * varName);
        virtual void exprNat(int32_t);
        virtual void exprCallBegin();
        virtual void exprCallEnd(uint32_t n);
        virtual void exprArray();
        virtual void exprPostIncrement();
        virtual void exprPreIncrement(); 
        virtual void exprPostDecrement();
        virtual void exprPreDecrement(); 
        virtual void exprAssignment(Constants::kind_t op);
        virtual void exprUnary(Constants::kind_t unaryop); 
        virtual void exprBinary(Constants::kind_t binaryop); 
        virtual void exprTernary(Constants::kind_t ternaryop, bool firstMissing);
        virtual void exprInlineIf(); 
        virtual void exprComma(); 
        virtual void exprDot(const char *);
        virtual void exprDeadlock();
        virtual void exprForAllBegin(const char *name);
        virtual void exprForAllEnd(const char *name);
        virtual void exprExistsBegin(const char *name);
        virtual void exprExistsEnd(const char *name);
    };
}

#endif

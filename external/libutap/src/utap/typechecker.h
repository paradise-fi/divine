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

#ifndef UTAP_TYPECHECKER_HH
#define UTAP_TYPECHECKER_HH


#include <exception>
#include <set>
#include "utap/system.h"
#include "utap/common.h"
#include "utap/expression.h"
#include "utap/statement.h"

namespace UTAP
{
    /** 
     * Visitor which collects all compile time computable
     * symbols. These are all global and template local constants and
     * all constant non-reference template parameters. Variables with
     * mixed storage are not considered compile time computable.
     */
    class CompileTimeComputableValues : public SystemVisitor
    {
    private:
        std::set<symbol_t> variables;
    public:
        virtual void visitVariable(variable_t &);
        virtual void visitInstance(instance_t &);
        bool contains(symbol_t) const;
    };

    /** 
     * A visitor which type checks the system it visits.  The type
     * checker can only visit the system given in the constructor. The
     * type checker must not be constructed before the system has been
     * parsed.
     */
    class TypeChecker : public SystemVisitor, public AbstractStatementVisitor
    {
    private:
        TimedAutomataSystem *system;
        CompileTimeComputableValues compileTimeComputableValues;
        function_t *function; /**< Current function being type checked. */

        template<class T>
        void handleError(T, std::string);
        template<class T>
        void handleWarning(T, std::string);
    
        expression_t checkInitialiser(type_t type, expression_t init);
        bool areAssignmentCompatible(type_t lvalue, type_t rvalue) const;
        bool areInlineIfCompatible(type_t thenArg, type_t elseArg) const;
        bool areEqCompatible(type_t t1, type_t t2) const;
        bool areEquivalent(type_t, type_t) const;
        bool isLValue(expression_t) const;
        bool isModifiableLValue(expression_t) const;
        bool isUniqueReference(expression_t expr) const;
        bool isParameterCompatible(type_t param, expression_t arg);
        bool checkParameterCompatible(type_t param, expression_t arg);
        void checkIgnoredValue(expression_t expr);
        bool checkAssignmentExpression(expression_t);
        bool checkConditionalExpressionInFunction(expression_t);

        bool isCompileTimeComputable(expression_t expr) const;
        void checkType(type_t, bool initialisable = false);

    public:
        TypeChecker(TimedAutomataSystem *system);
        virtual ~TypeChecker() {}
        virtual void visitSystemAfter(TimedAutomataSystem *);
        virtual void visitVariable(variable_t &);
        virtual void visitState(state_t &);
        virtual void visitEdge(edge_t &);
        virtual void visitInstance(instance_t &);
        virtual void visitProperty(expression_t);
        virtual void visitFunction(function_t &);
        virtual void visitProgressMeasure(progress_t &);
        virtual void visitProcess(instance_t &);

        virtual int32_t visitEmptyStatement(EmptyStatement *stat);
        virtual int32_t visitExprStatement(ExprStatement *stat);
        virtual int32_t visitAssertStatement(AssertStatement *stat);
        virtual int32_t visitForStatement(ForStatement *stat);
        virtual int32_t visitIterationStatement(IterationStatement *stat);
        virtual int32_t visitWhileStatement(WhileStatement *stat);
        virtual int32_t visitDoWhileStatement(DoWhileStatement *stat);
        virtual int32_t visitBlockStatement(BlockStatement *stat);
        virtual int32_t visitIfStatement(IfStatement *stat);
        virtual int32_t visitReturnStatement(ReturnStatement *stat);

        /** Type check an expression */
        bool checkExpression(expression_t);
    };
}

#endif

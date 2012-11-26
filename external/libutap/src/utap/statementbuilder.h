// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-

/* libutap - Uppaal Timed Automata Parser.
   Copyright (C) 2002-2004 Uppsala University and Aalborg University.
   
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

#ifndef UTAP_STATEMENTBUILDER_H
#define UTAP_STATEMENTBUILDER_H

#include <cassert>
#include <vector>
#include <inttypes.h>

#include "utap/expressionbuilder.h"
#include "utap/utap.h"

namespace UTAP
{
    /**
     * Partial implementation of the builder interface, useful
     * for building something with statements that is not a UTAP system.
     */
    class StatementBuilder : public ExpressionBuilder
    {
    protected:
        /** 
         * The \a params frame is used temporarily during parameter
         * parsing.
         */
        frame_t params;

        /** The function currently being parsed. */
        function_t *currentFun;

        /** Stack of nested statement blocks. */
        std::vector<BlockStatement*> blocks; 

        /** The types of a struct. */
        std::vector<type_t> fields;

        /** The labels of a struct. */
        std::vector<std::string> labels;

        virtual variable_t *addVariable(type_t type, const char*  name,
                                        expression_t init) = 0;
        virtual bool addFunction(type_t type, const char* name) = 0;

        static void collectDependencies(std::set<symbol_t> &, expression_t );
        static void collectDependencies(std::set<symbol_t> &, type_t );

    public:
        StatementBuilder(TimedAutomataSystem *);

        virtual void typeArrayOfSize(size_t);
        virtual void typeArrayOfType(size_t);
        virtual void typeStruct(PREFIX, uint32_t fields);
        virtual void structField(const char* name); 
        virtual void declTypeDef(const char* name); 
        virtual void declVar(const char* name, bool init); 
        virtual void declInitialiserList(uint32_t num); 
        virtual void declFieldInit(const char* name); 
        virtual void declParameter(const char* name, bool);
        virtual void declFuncBegin(const char* name);
        virtual void declFuncEnd();
        virtual void blockBegin();
        virtual void blockEnd();
        virtual void emptyStatement();
        virtual void forBegin();
        virtual void forEnd(); 
        virtual void iterationBegin(const char *name);
        virtual void iterationEnd(const char *name);
        virtual void whileBegin();
        virtual void whileEnd();
        virtual void doWhileBegin();
        virtual void doWhileEnd();
        virtual void ifBegin();
        virtual void ifElse();
        virtual void ifEnd(bool); 
        virtual void exprStatement(); 
        virtual void returnStatement(bool);
        virtual void assertStatement();
        virtual void exprCallBegin();
    };
}
#endif

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

#ifndef UTAP_ABSTRACTBUILDER_HH
#define UTAP_ABSTRACTBUILDER_HH

#include <exception>
#include <string>

#include "utap/builder.h"
#include "utap/utap.h"
#include "utap/position.h"

namespace UTAP
{
    class NotSupportedException : public std::exception 
    {
    private:
        std::string error;
    public:
        NotSupportedException(const char *err) { error = err; }
        virtual ~NotSupportedException() throw() {}
        virtual const char* what() const throw() { return error.c_str(); }
    };

    class AbstractBuilder : public ParserBuilder
    {
    protected:
        position_t position;
    public:
        AbstractBuilder();

        virtual void setPosition(uint32_t, uint32_t);

        /************************************************************
         * Query functions
         */
        virtual bool isType(const char*);

        /************************************************************
         * Types
         */
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
        virtual void typeStruct(PREFIX, uint32_t fields);
        virtual void typeArrayOfSize(size_t);
        virtual void typeArrayOfType(size_t);
        virtual void structField(const char* name); 
        virtual void declTypeDef(const char* name); 

        /************************************************************
         * Variable declarations
         */
        virtual void declVar(const char* name, bool init); 
        virtual void declInitialiserList(uint32_t num); // n initialisers
        virtual void declFieldInit(const char* name); // 1 initialiser

        /************************************************************
         * Progress measure declarations
         */
        virtual void declProgress(bool);
        
        /************************************************************
         * Function declarations
         */
        virtual void declParameter(const char* name, bool);
        virtual void declFuncBegin(const char* name); // n paramaters
        virtual void declFuncEnd(); // 1 block

        /************************************************************
         * Process declarations
         */
        virtual void procBegin(const char* name);
        virtual void procEnd(); // 1 ProcBody
        virtual void procState(const char* name, bool hasInvariant); // 1 expr
        virtual void procStateCommit(const char* name); // mark previously decl. state
        virtual void procStateUrgent(const char* name); // mark previously decl. state
        virtual void procStateInit(const char* name); // mark previously decl. state
        virtual void procEdgeBegin(const char* from, const char* to, const bool control);
        virtual void procEdgeEnd(const char* from, const char* to); 
        // 1 epxr,1sync,1expr
        virtual void procSelect(const char *id);
        virtual void procGuard();
        virtual void procSync(Constants::synchronisation_t type); // 1 expr
        virtual void procUpdate();
    
        /************************************************************
         * Statements
         */
        virtual void blockBegin();
        virtual void blockEnd();
        virtual void emptyStatement();
        virtual void forBegin(); // 3 expr
        virtual void forEnd(); // 1 stat
        virtual void iterationBegin(const char *name); // 1 id, 1 type
        virtual void iterationEnd(const char *name); // 1 stat
        virtual void whileBegin();
        virtual void whileEnd(); // 1 expr, 1 stat
        virtual void doWhileBegin();
        virtual void doWhileEnd(); // 1 stat, 1 expr
        virtual void ifBegin();
        virtual void ifElse();
        virtual void ifEnd(bool); // 1 expr, 1 or 2 statements
        virtual void breakStatement();
        virtual void continueStatement();
        virtual void switchBegin();
        virtual void switchEnd(); // 1 expr, 1+ case/default
        virtual void caseBegin();
        virtual void caseEnd();  // 1 expr, 0+ stat
        virtual void defaultBegin();
        virtual void defaultEnd(); // 0+ statements
        virtual void exprStatement(); // 1 expr
        virtual void returnStatement(bool); // 1 expr if argument is true
        virtual void assertStatement();
    
        /************************************************************
         * Expressions
         */
        virtual void exprTrue();
        virtual void exprFalse();
        virtual void exprId(const char * varName);
        virtual void exprNat(int32_t); // natural number
        virtual void exprCallBegin();
        virtual void exprCallEnd(uint32_t n); // n exprs as arguments
        virtual void exprArray(); // 2 expr 
        virtual void exprPostIncrement(); // 1 expr
        virtual void exprPreIncrement(); // 1 expr
        virtual void exprPostDecrement(); // 1 expr
        virtual void exprPreDecrement(); // 1 expr
        virtual void exprAssignment(Constants::kind_t op); // 2 expr
        virtual void exprUnary(Constants::kind_t unaryop); // 1 expr
        virtual void exprBinary(Constants::kind_t binaryop); // 2 expr
        virtual void exprTernary(Constants::kind_t ternaryop, bool firstMissing); // 3 expr
        virtual void exprInlineIf(); // 3 expr
        virtual void exprComma(); // 2 expr
        virtual void exprDot(const char *); // 1 expr
        virtual void exprDeadlock();
        virtual void exprForAllBegin(const char *name);
        virtual void exprForAllEnd(const char *name);
        virtual void exprExistsBegin(const char *name);
        virtual void exprExistsEnd(const char *name);
    
        /************************************************************
         * System declaration
         */
        virtual void instantiationBegin(const char*, size_t, const char*);
        virtual void instantiationEnd(
            const char *, size_t, const char *, size_t); 
        virtual void process(const char*);
        virtual void done();

        /************************************************************
         * Properties
         */
        virtual void property();
        virtual void paramProperty(size_t, Constants::kind_t);
        virtual void declParamId(const char*);

        /********************************************************************
         * Guiding
         */
        virtual void beforeUpdate();
        virtual void afterUpdate();


        /********************************************************************
         * Priority
         */
        virtual void incProcPriority();
        virtual void incChanPriority();
        virtual void chanPriority();
        virtual void procPriority(const char*);
        virtual void defaultChanPriority();

        /********************************************************************
         * SSQL
         */
        virtual void ssQueryBegin();
        virtual void ssQueryEnd();
    };
}
#endif

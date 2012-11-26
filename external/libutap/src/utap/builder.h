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

#ifndef UTAP_BUILDER_HH
#define UTAP_BUILDER_HH

#include <cstdio>
#include <stdexcept>
#include <string>
#include <boost/format.hpp>

#include "utap/common.h"

namespace UTAP
{
    /**
     * Exception indicating a type error. This is thrown by
     * implementations of the ParserBuilder interface to indicate that
     * a type error has occurred.
     */
    class TypeException : public std::runtime_error
    {
    public:
        TypeException(std::string);
        TypeException(const boost::format &);
    };

    /**
     * The ParserBuilder interface is used by the parser to output the
     * parsed system. The parser itself will only parse the system -
     * it will not actually store or otherwise process the
     * input. Instead, the parser is configured with an implementation
     * of the ParserBuilder interface. Productions in the BNF
     * implemented by the parser correspond to methods in the
     * ParserBuilder interface.
     * 
     * Errors (such as type errors) can be reported back to the parser
     * by either throwing a TypeException or by calling an error
     * method in the ErrorHandler that has been set by a call to
     * setErrorHandler().
     *
     * <h3>Expressions</h3>
     *
     * Expressions are reported in reverse polish notation using the
     * exprXXX methods.
     *
     * <h3>Declarations</h3>
     *
     * The proper protocol for declaring a new type name is to
     *
     * - report the type using the typeXXX methods
     * - call declTypeDef(name) to declare the type-name
     *
     * The proper protocol for declaring a variable is to
     *
     * - report the type using the typeXXX methods
     * - optionally report an expression for the initialiser
     * - call declVar(name, init), where init is true if and
     *   only if an initialiser has been reported
     */
    class ParserBuilder
    {
    public:
        /*********************************************************************
         * Prefixes
         */
        enum PREFIX { PREFIX_NONE = 0,
                      PREFIX_CONST = 1,
                      PREFIX_URGENT = 2,
                      PREFIX_BROADCAST = 4,
                      PREFIX_URGENT_BROADCAST = 6,
                      PREFIX_META = 8 };

        virtual ~ParserBuilder() {}

        /** 
         * Add mapping from an absolute position to a relative XML
         * element.
         */
        virtual void addPosition(
            uint32_t position, uint32_t offset, uint32_t line, std::string path) = 0;

        /**
         * Sets the current position. The current position indicates
         * where in the input file the current productions can be
         * found.
         */
        virtual void setPosition(uint32_t a, uint32_t b) = 0;

        // Called when an error is detected
        virtual void handleError(std::string) = 0;

        // Called when a warning is issued
        virtual void handleWarning(std::string) = 0;

        void handleWarning(const char *msg, ...);

        void handleError(const char *msg, ...);
    
        /**
         * Must return true if and only if name is registered in the
         * symbol table as a named type, for instance, "int" or "bool" or
         * a user defined type.
         */
        virtual bool isType(const char*) = 0;

        /** Duplicate type at the top of the type stack. */
        virtual void typeDuplicate() = 0;

        /** Pop type at the topof the type stack. */
        virtual void typePop() = 0;

        /** 
         * Called whenever a boolean type is parsed. 
         */
        virtual void typeBool(PREFIX) = 0;
        
        /** 
         * Called whenever an integer type is parsed.
         */
        virtual void typeInt(PREFIX) = 0;

        /** 
         * Called whenever an integer type with a range is
         * parsed. Expressions for the lower and upper have been
         * pushed before.
         */
        virtual void typeBoundedInt(PREFIX) = 0;

        /** 
         * Called whenever a channel type is parsed.
         */
        virtual void typeChannel(PREFIX) = 0;

        /** 
         * Called whenever a clock type is parsed.
         */
        virtual void typeClock() = 0;

        /** 
         * Called whenever a void type is parsed.
         */
        virtual void typeVoid() = 0;
        
        /**
         * Called to create an array type. The size of the array was
         * previously pushed as an expression.
         */
        virtual void typeArrayOfSize(size_t) = 0;

        /**
         * Called to create an array type. The size of the array was
         * previously pushed as a type.
         */
        virtual void typeArrayOfType(size_t) = 0;

        /** 
         * Called whenever a scalar type is parsed. The size of the
         * scalar set was pushed as an expression before.
         */
        virtual void typeScalar(PREFIX) = 0;

        /**
         * Called when a type name has been parsed. Prefix indicates
         * whether the type named was prefixed (e.g. with 'const').
         */
        virtual void typeName(PREFIX, const char* name) = 0;

        /**
         * Called when a struct-type has been parsed. Prior to the
         * call 'fields' fields must have been declared using the
         * structXXX methods.
         */
        virtual void typeStruct(PREFIX, uint32_t fields) = 0;

        /**
         * Called to declare a field of a structure. The type of the
         * field has been reported using a typeXXX method prior to the
         * call of structField(). In case of array fields, 'dim'
         * expressions indicating the array sizes have been reported.
         */
        virtual void structField(const char* name) = 0; 

        /** 
         * Used when a typedef declaration was parsed. name is the
         * name of the new type.
         */
        virtual void declTypeDef(const char* name) = 0; 

        /**
         * Called to when a variable declaration has been parsed. 
         */
        virtual void declVar(const char* name, bool init) = 0; 

        virtual void declInitialiserList(uint32_t num) = 0; // n initialisers
        virtual void declFieldInit(const char* name) = 0; // 1 initialiser
        
        /**
         * Guard progress measure declaration. Requires two
         * expressions if \a hasGuard is true, otherwise one.
         */
        virtual void declProgress(bool hasGuard) = 0;
  
        /********************************************************************
         * Function declarations
         */
        virtual void declParameter(const char* name, bool ref) = 0;
  
        virtual void declFuncBegin(const char* name) = 0;
        virtual void declFuncEnd() = 0; // 1 block

        /********************************************************************
         * Process declarations
         */
        virtual void procBegin(const char* name) = 0; // m parameters
        virtual void procEnd() = 0; // 1 ProcBody
        virtual void procState(const char* name, bool hasInvariant) = 0; // 1 expr
        virtual void procStateCommit(const char* name) = 0; // mark previously decl. state
        virtual void procStateUrgent(const char* name) = 0; // mark previously decl. state
        virtual void procStateInit(const char* name) = 0; // mark previously decl. state
        virtual void procEdgeBegin(const char* from, const char* to, const bool control) = 0;
        virtual void procEdgeEnd(const char* from, const char* to) = 0; 
        virtual void procSelect(const char * id) = 0; // 1 expr
        virtual void procGuard() = 0; // 1 expr
        virtual void procSync(Constants::synchronisation_t type) = 0; // 1 expr
        virtual void procUpdate() = 0; // 1 expr

        /*********************************************************************
         * Statements
         */
        virtual void blockBegin() = 0;
        virtual void blockEnd() = 0;
        virtual void emptyStatement() = 0;
        virtual void forBegin() = 0; // 3 expr
        virtual void forEnd() = 0; // 1 stat
        virtual void iterationBegin(const char *name) = 0; // 1 id, 1 type
        virtual void iterationEnd(const char *name) = 0; // 1 stat
        virtual void whileBegin() = 0;
        virtual void whileEnd() = 0; // 1 expr, 1 stat
        virtual void doWhileBegin() = 0;
        virtual void doWhileEnd() = 0; // 1 stat, 1 expr
        virtual void ifBegin() = 0;
        virtual void ifElse() = 0;
        virtual void ifEnd(bool) = 0; // 1 expr, 1 or 2 statements
        virtual void breakStatement() = 0;
        virtual void continueStatement() = 0;
        virtual void switchBegin() = 0;
        virtual void switchEnd() = 0; // 1 expr, 1+ case/default
        virtual void caseBegin() = 0;
        virtual void caseEnd() = 0;  // 1 expr, 0+ stat
        virtual void defaultBegin() = 0;
        virtual void defaultEnd() = 0; // 0+ statements
        virtual void exprStatement() = 0; // 1 expr
        virtual void returnStatement(bool) = 0; // 1 expr if argument is true
        virtual void assertStatement() = 0; // 1 expr

        /********************************************************************
         * Expressions
         */
        virtual void exprFalse() = 0;
        virtual void exprTrue() = 0;
        virtual void exprId(const char * varName) = 0;
        virtual void exprNat(int32_t) = 0; // natural number
        virtual void exprCallBegin() = 0;
        virtual void exprCallEnd(uint32_t n) = 0; // n exprs as arguments
        virtual void exprArray() = 0; // 2 expr 
        virtual void exprPostIncrement() = 0; // 1 expr
        virtual void exprPreIncrement() = 0; // 1 expr
        virtual void exprPostDecrement() = 0; // 1 expr
        virtual void exprPreDecrement() = 0; // 1 expr
        virtual void exprAssignment(Constants::kind_t op) = 0; // 2 expr
        virtual void exprUnary(Constants::kind_t unaryop) = 0; // 1 expr
        virtual void exprBinary(Constants::kind_t binaryop) = 0; // 2 expr
        virtual void exprTernary(Constants::kind_t ternaryop, bool firstMissing = false) = 0; // 3 expr
        virtual void exprInlineIf() = 0; // 3 expr
        virtual void exprComma() = 0; // 2 expr
        virtual void exprDot(const char *) = 0; // 1 expr
        virtual void exprDeadlock() = 0;
        virtual void exprForAllBegin(const char *name) = 0;
        virtual void exprForAllEnd(const char *name) = 0;
        virtual void exprExistsBegin(const char *name) = 0;
        virtual void exprExistsEnd(const char *name) = 0;

        /********************************************************************
         * System declaration
         */
        virtual void instantiationBegin(
            const char* id, size_t parameters, const char* templ) = 0;
        virtual void instantiationEnd(
            const char* id, size_t parameters, const char* templ, size_t arguments) = 0;
        virtual void process(const char*) = 0;
        virtual void done() = 0; // marks the end of the file

        /********************************************************************
         * Properties
         */
        virtual void property() = 0;
        virtual void paramProperty(size_t, Constants::kind_t) = 0;
        virtual void declParamId(const char*) = 0;

        /********************************************************************
         * Guiding
         */
        virtual void beforeUpdate() = 0;
        virtual void afterUpdate() = 0;

        /********************************************************************
         * Priority
         */
        virtual void incProcPriority() = 0;
        virtual void incChanPriority() = 0;
        virtual void chanPriority() = 0;
        virtual void procPriority(const char*) = 0;
        virtual void defaultChanPriority() = 0;

        /********************************************************************
         * SSQL
         */
        virtual void ssQueryBegin() = 0;
        virtual void ssQueryEnd() = 0;
    };
}
    
/**
 * Parse a file in the XTA format, reporting the system to the given
 * implementation of the the ParserBuilder interface and reporting
 * errors to the ErrorHandler. If newxta is true, then the 4.x syntax
 * is used; otherwise the 3.x syntax is used. On success, this
 * function returns with a positive value.
 */
int32_t parseXTA(FILE *, UTAP::ParserBuilder *, bool newxta);

int32_t parseXTA(const char*, UTAP::ParserBuilder *, bool newxta);

/**
 * Parse a buffer in the XTA format, reporting the system to the given
 * implementation of the the ParserBuilder interface and reporting
 * errors to the ErrorHandler. If newxta is true, then the 4.x syntax
 * is used; otherwise the 3.x syntax is used. On success, this
 * function returns with a positive value.
 */
int32_t parseXTA(const char*, UTAP::ParserBuilder *, 
                 bool newxta, UTAP::xta_part_t part, std::string xpath);


/**
 * Parse a buffer in the XML format, reporting the system to the given
 * implementation of the the ParserBuilder interface and reporting
 * errors to the ErrorHandler. If newxta is true, then the 4.x syntax
 * is used; otherwise the 3.x syntax is used. On success, this
 * function returns with a positive value.
 */
int32_t parseXMLBuffer(const char *buffer, UTAP::ParserBuilder *,
                       bool newxta);

/**
 * Parse the file with the given name assuming it is in the XML
 * format, reporting the system to the given implementation of the the
 * ParserBuilder interface and reporting errors to the
 * ErrorHandler. If newxta is true, then the 4.x syntax is used;
 * otherwise the 3.x syntax is used. On success, this function returns
 * with a positive value.
 */
int32_t parseXMLFile(const char *filename, UTAP::ParserBuilder *, bool newxta);

/**
 * Parse properties from a buffer. The properties are reported using
 * the given ParserBuilder and errors are reported using the
 * ErrorHandler.
 * State space queries can be enabled by passing 'true'
 * for the 'ssql' parameter.
 */
int32_t parseProperty(const char *str,
                      UTAP::ParserBuilder *aParserBuilder, bool ssql = false);

/**
 * Parse properties from a file. The properties are reported using the
 * given ParserBuilder and errors are reported using the ErrorHandler.
 * State space queries can be enabled by passing 'true'
 * for the 'ssql' parameter.
 */
int32_t parseProperty(FILE *, UTAP::ParserBuilder *aParserBuilder, bool ssql = false);


#endif

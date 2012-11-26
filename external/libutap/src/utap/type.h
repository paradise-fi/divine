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

#ifndef UTAP_TYPE_HH
#define UTAP_TYPE_HH

#include <inttypes.h>
#include <string>

#include "utap/common.h"
#include "utap/position.h"

namespace UTAP
{
    class expression_t;
    class frame_t;
    class symbol_t;

    /**
       A reference to a type.

       Types are represented as trees of types. The type cannot be
       access directly. You need to use an instance of type_t to
       access a type. Internally, types are reference counted and do
       not need to be deallocated manually. Types are immutable.

       Types are either primitive such as clocks or channels, or
       contructed types such as structs and array. Constructed types
       are created using one of the factory methods in the type_t
       class. Primitive types are leaves in the tree, constructed
       types are inner nodes.

       All types have a kind - a type for the type. The kind is a
       value of kind_t. In anticipation of a future homogeneous AST,
       this kind is defined in the same enumerable as the kind of
       expressions.
       
       Some constructed types are considered prefixes for other types:
       URGENT, COMMITTED, BROADCAST, CONSTANT and BROADCAST.

       LABEL types are references to named types. These are introduced
       by references to types defined with a typedef or by scalar set
       declarations. They have one child: The type which has been
       named.

       Constructed types are:

       - ARRAY; the first child is the element type, the second type
         is the array size (another type; either integer or scalarset).

       - RECORD; the children are the fields of the record.

       - FUNCTION; the first child is the return type. Remaining
         children are parameters.

       - INSTANCE; the parameters of an instance are the children.

       - PROCESS; the local variables of a process are the children.

       - PROCESSSET; process with unbound parameters. The unbound 
         parameters are the children.

       - TYPEDEF; a type definition - it has one child.

       - RANGE; ranges have three children: the first is the type on
         which a range has been applied, the second is the lower bound
         and the third is the upper bound (the second and third
         children are really expression objects wrapped in a dummy
         type).

       - REF; a reference - the first child is the type from which the
         reference type is formed.
    */
    class type_t
    {
    private:
        struct child_t;
        struct type_data;
        type_data *data;

        explicit type_t(Constants::kind_t kind, 
                        const position_t &pos, size_t size);
    public:
        /** 
         * Default constructor. This creates a null-type.
         */
        type_t();

        /** Copy constructor. */
        type_t(const type_t &);

        /** Destructor. */
        ~type_t();

        /** Assignment operator. */
        const type_t &operator = (const type_t &);

        /** Equality operator. */
        bool operator == (const type_t &) const;

        /** Inequality operator. */
        bool operator != (const type_t &) const;

        /** Returns the kind of type object. */
        Constants::kind_t getKind() const;

        /** 
         * Returns the position of the type in the input file. This
         * exposes the fact that the type is actually part of the AST.
         */
        position_t getPosition() const;

        /**
         * Returns the number of children.
         */
        size_t size() const;

        /** Less-than operator. */
        bool operator < (const type_t &) const;

        /** Returns the \a i'th child. */
        const type_t operator[](uint32_t) const;

        /** Returns the \a i'th child. */
        const type_t get(uint32_t) const;        

        /** Returns the \a i'th label. */
        const std::string &getLabel(uint32_t) const;

        /** Returns the expression associated with the type. */
        expression_t getExpression() const;

        /**
         * Returns the size of an array (this is itself a type). @pre
         * isArray().
         */
        type_t getArraySize() const;

        /**
         * Returns the element type of an array. Preserves any
         * prefixes. @pre isArray();
         */
        type_t getSub() const;

        /** 
         * Returns the \i'th field of a record or process. Preserves
         * any prefixes. @pre isRecord() or isProcess().
         */
        type_t getSub(size_t) const;

        /**
         * Returns the number of fields of a record. @pre isRecord().
         */
        size_t getRecordSize() const;
        
        /**
         * Returns the label of the \i'th field of a record. @pre
         * isRecord().
         */
        std::string getRecordLabel(size_t i) const;

        /** 
         * Returns the index of the record or process field with the
         * given label. Returns -1 if such a field does not
         * exist. @pre isRecord() or isProcess().
         */
        int32_t findIndexOf(std::string) const;

        /**
         * Returns the range of a RANGE type. @pre isRange().
         */
        std::pair<expression_t, expression_t> getRange() const;

        /** Generates string representation of the type. */
        std::string toString() const;

        /** Shortcut for is(INT). */
        bool isInteger() const { return is(Constants::INT); }

        /** Shortcut for is(BOOL). */
        bool isBoolean() const { return is(Constants::BOOL); }
        
        /** Shortcut for is(FUNCTION). */
        bool isFunction() const { return is(Constants::FUNCTION); }

        /** Shortcut for is(PROCESS). */
        bool isProcess() const { return is(Constants::PROCESS); }

        /** Shortcut for is(PROCESSSET). */
        bool isProcessSet() const { return is(Constants::PROCESSSET); }

        /** Shortcut for is(LOCATION). */
        bool isLocation() const { return is(Constants::LOCATION); }

        /** Shortcut for is(CHANNEL). */
        bool isChannel() const { return is(Constants::CHANNEL); }

        /** Shortcut for is(ARRAY). */
        bool isArray() const { return is(Constants::ARRAY); }

        /** Shortcut for is(SCALAR). */
        bool isScalar() const { return is(Constants::SCALAR); }

        /** Shortcut for is(CLOCK). */
        bool isClock() const { return is(Constants::CLOCK); }

        /** Shortcut for is(RECORD). */
        bool isRecord() const { return is(Constants::RECORD); }
        
        /** Shortcut for is(DIFF). */
        bool isDiff() const { return is(Constants::DIFF); }

        /** Shortcut for is(VOID_TYPE). */
        bool isVoid() const { return is(Constants::VOID_TYPE); }
        
        /** Shortcut for is(COST). */
        bool isCost() const { return is(Constants::COST); }

        /** 
         * Returns true if this is a boolean or integer. Shortcut for
         * isInt() || isBoolean().
         */
        bool isIntegral() const;

        /** 
         * Returns true if this is an invariant, boolean or
         * integer. Shortcut for isIntegral() || is(INVARIANT).
         */
        bool isInvariant() const;

        /** 
         * Returns true if this is a guard, invariant, boolean or
         * integer.  Shortcut for is(GUARD) || isInvariant().
         */
        bool isGuard() const;

        /**
         * Returns true if this is a constraint, guard, invariant,
         * boolean or integer. Shortcut for is(CONSTRAINT) ||
         * isGuard().
         */
        bool isConstraint() const;

        /**
         * Returns true if this is a formula, constraint, guard,
         * invariant, boolean or integer. Shortcut for is(FORMULA) ||
         * isConstraint().
         */
        bool isFormula() const;

        /**
         * Removes any leading prefixes, RANGE, REF and LABEL types
         * and returns the result.
         */
        type_t strip() const;

        /**
         * Removes any leading prefixes, RANGE, REF, LABEL and ARRAY
         * types and returns the result.
         */
        type_t stripArray() const;

        /**
         * Returns false for non-prefix types and true
         * otherwise. Non-prefix types are PRIMITIVE, ARRAY, RECORD,
         * PROCESS, TEMPLATE, FUNCTION, INSTANCE, RANGE, REF, TYPEDEF.
         */
        bool isPrefix() const;

        /**
         * Returns true if and only if all elements of the type are
         * constant.
         */
        bool isConstant() const;


        /**
         * Returns true if and only if all elements of the type are
         * not constant.
         */
        bool isNonConstant() const;

        /**
         * Returns true if the type has kind \a kind or if type is a
         * prefix, RANGE or REF type and the getChild().is(kind)
         * returns true.
         */
        bool is(Constants::kind_t kind) const;

        /**
         * Returns true if this is null-type or of kind UNKNOWN.
         */
        bool unknown() const;

        /**
         * Replaces any LABEL labeled \a from occuring in the type
         * with a LABEL \a to. As always, a type is immutable, so a
         * copy of the type will be created.
         */
        type_t rename(std::string from, std::string to) const;

        /**
         * Substitutes any occurence of \a symbol in any expression in
         * the type (expressions that occur as ranges either on
         * array sizes, scalars or integers) with \a expr.
         */
        type_t subst(symbol_t symbol, expression_t expr) const;
        /**
         * Creates a new type by adding a prefix to it. The prefix
         * could be anything and it is the responsibility of the
         * caller to make sure that the given kind is a valid prefix.
         */
        type_t createPrefix(Constants::kind_t kind, position_t = position_t()) const;

        /** Creates a LABEL. */
        type_t createLabel(std::string, position_t = position_t()) const;

        /**  */
        type_t createPosition(position_t = position_t()) const;

        /** 
         */
        static type_t createRange(type_t, expression_t, expression_t, 
                                  position_t = position_t());

        /** Create a primitive type. */
        static type_t createPrimitive(Constants::kind_t,
                                      position_t = position_t());

        /** Creates an array type. */
        static type_t createArray(type_t sub, type_t size, position_t = position_t());

        /** Creates a new type definition. */
        static type_t createTypeDef(std::string, type_t, position_t = position_t());

        /** Creates a new process type. */
        static type_t createProcess(frame_t, position_t = position_t());

        /** Creates a new processset type. */
        static type_t createProcessSet(type_t instance, position_t = position_t());

        /** Creates a new record type */
        static type_t createRecord(const std::vector<type_t> &, 
                                   const std::vector<std::string> &, 
                                   position_t = position_t());

        /** Creates a new function type */
        static type_t createFunction(type_t, 
                                     const std::vector<type_t> &, 
                                     const std::vector<std::string> &, 
                                     position_t = position_t());

        /** Creates a new instance type */
        static type_t createInstance(frame_t, position_t = position_t());
    };
}

std::ostream &operator << (std::ostream &o, UTAP::type_t t);

#endif

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

#ifndef UTAP_COMMON_HH
#define UTAP_COMMON_HH

#ifdef __MINGW32__
#include <stdint.h>
#else
#include <inttypes.h>
#endif
#include <string>
#include <vector>
#include <cstring>

namespace UTAP
{
    namespace Constants
    {
        enum kind_t 
        {
            PLUS = 0,
            MINUS = 1,
            MULT = 2,
            DIV = 3,
            MOD = 4,
            BIT_AND = 5,
            BIT_OR = 6,
            BIT_XOR = 7,
            BIT_LSHIFT = 8,
            BIT_RSHIFT = 9,
            AND = 10,
            OR = 11,
            MIN = 12,
            MAX = 13,
            RATE = 14,

            /********************************************************
             * Relational operators
             */
            LT = 20,
            LE = 21,
            EQ = 22,
            NEQ = 23,
            GE = 24,
            GT = 25,

            /********************************************************
             * Unary operators
             */
            NOT = 30,
            FORALL = 31,
            EXISTS = 32,

            /********************************************************
             * Assignment operators
             */
            ASSIGN = 40,
            ASSPLUS = 41,
            ASSMINUS = 42,
            ASSDIV = 43,
            ASSMOD = 44,
            ASSMULT = 45,
            ASSAND = 46,
            ASSOR = 47,
            ASSXOR = 48,
            ASSLSHIFT = 49,
            ASSRSHIFT = 50,

            /*******************************************************
             * CTL Quantifiers
             */
            EF = 60,
            EG = 61,
            AF = 62,
            AG = 63,
            LEADSTO = 64,
            A_UNTIL = 65,
            A_WEAKUNTIL = 66,
            EF2 = 67,
            EG2 = 68,
            AF2 = 69,
            AG2 = 70,
            AG_R = 71,
            EF_R = 72,

            /*******************************************************
             * Control Synthesis Operator
             */
            CONTROL = 80,
            EF_CONTROL = 81,
            CONTROL_TOPT = 82,

            /*******************************************************
             * Parameter generation
             */
            SUP = 83,
            INF = 84,
            
            /*******************************************************
             * Additional constants used by ExpressionProgram's and
             * the TypeCheckBuilder (but not by the parser, although
             * some of then ought to be used, FIXME).
             */
            IDENTIFIER = 512,
            CONSTANT = 513,
            ARRAY = 514,
            POSTINCREMENT = 515,
            PREINCREMENT = 516,
            POSTDECREMENT = 517,
            PREDECREMENT = 518,
            UNARY_MINUS = 519,
            LIST = 520,
            DOT = 521,
            INLINEIF = 522,
            COMMA = 523,
            SYNC = 525,
            DEADLOCK = 526,
            FUNCALL = 527,

            /*******************************************************
             * Types
             */
            UNKNOWN = 600,
            VOID_TYPE = 601,
            CLOCK = 602,
            INT = 603,
            BOOL = 604,
            SCALAR = 605,
            LOCATION = 606,
            CHANNEL = 607,
            COST = 608,
            INVARIANT = 609,
            INVARIANT_WR = 610,
            GUARD = 611,
            DIFF = 612,
            CONSTRAINT= 613,
            FORMULA = 614,

            RANGE = 650,
            LABEL = 651,
            RECORD = 652,
            REF = 654,
            URGENT = 655,
            COMMITTED = 656,
            BROADCAST = 657,
            TYPEDEF = 658,
            PROCESS = 659,
            PROCESSSET = 660,
            INSTANCE = 661,
            META = 662,
            FUNCTION = 663
        };

        /**********************************************************
         * Synchronisations:
         */
        enum synchronisation_t 
        {
            SYNC_QUE = 0,
            SYNC_BANG = 1
        };
    }

    /** Type for specifying which XTA part to parse (syntax switch) */
    typedef enum 
    { 
        S_XTA, // entire system 
        S_DECLARATION, S_LOCAL_DECL, S_INST, S_SYSTEM, S_PARAMETERS, 
        S_INVARIANT, S_SELECT, S_GUARD, S_SYNC, S_ASSIGN, 
        S_EXPRESSION, S_PROPERTY
    } xta_part_t;

}

#endif


/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton interface for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     T_NO_TOKEN = 258,
     T_FOREIGN_ID = 259,
     T_FOREIGN_SQUARE_BRACKETS = 260,
     T_EXCLAM = 261,
     T_CONST = 262,
     T_TRUE = 263,
     T_FALSE = 264,
     T_SYSTEM = 265,
     T_PROCESS = 266,
     T_STATE = 267,
     T_CHANNEL = 268,
     T_COMMIT = 269,
     T_INIT = 270,
     T_ACCEPT = 271,
     T_ASSERT = 272,
     T_TRANS = 273,
     T_GUARD = 274,
     T_SYNC = 275,
     T_ASYNC = 276,
     T_PROPERTY = 277,
     T_EFFECT = 278,
     T_BUCHI = 279,
     T_GENBUCHI = 280,
     T_STREETT = 281,
     T_RABIN = 282,
     T_MULLER = 283,
     T_ID = 284,
     T_INT = 285,
     T_BYTE = 286,
     T_NAT = 287,
     T_WIDE_ARROW = 288,
     T_ASSIGNMENT = 289,
     T_IMPLY = 290,
     T_BOOL_AND = 291,
     T_BOOL_OR = 292,
     T_XOR = 293,
     T_AND = 294,
     T_OR = 295,
     T_NEQ = 296,
     T_EQ = 297,
     T_GT = 298,
     T_GEQ = 299,
     T_LEQ = 300,
     T_LT = 301,
     T_RSHIFT = 302,
     T_LSHIFT = 303,
     T_PLUS = 304,
     T_MINUS = 305,
     T_MOD = 306,
     T_DIV = 307,
     T_MULT = 308,
     UOPERATOR = 309,
     T_DECREMENT = 310,
     T_INCREMENT = 311,
     T_UNARY_MINUS = 312,
     T_BOOL_NOT = 313,
     T_TILDE = 314,
     T_ARROW = 315,
     T_DOT = 316,
     T_SQUARE_BRACKETS = 317,
     T_PARENTHESIS = 318
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 1676 of yacc.c  */
#line 103 "dve_grammar.yy"

  bool flag;
  int number;
  char string[MAXLEN];



/* Line 1676 of yacc.c  */
#line 123 "dve_grammar.tab.hh"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE dve_yylval;

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif

extern YYLTYPE dve_yylloc;


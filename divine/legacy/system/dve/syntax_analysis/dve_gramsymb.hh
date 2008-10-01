/* A Bison parser, made by GNU Bison 2.0.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

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
#define T_NO_TOKEN 258
#define T_FOREIGN_ID 259
#define T_FOREIGN_SQUARE_BRACKETS 260
#define T_EXCLAM 261
#define T_CONST 262
#define T_TRUE 263
#define T_FALSE 264
#define T_SYSTEM 265
#define T_PROCESS 266
#define T_STATE 267
#define T_CHANNEL 268
#define T_COMMIT 269
#define T_INIT 270
#define T_ACCEPT 271
#define T_ASSERT 272
#define T_TRANS 273
#define T_GUARD 274
#define T_SYNC 275
#define T_ASYNC 276
#define T_PROPERTY 277
#define T_EFFECT 278
#define T_BUCHI 279
#define T_GENBUCHI 280
#define T_STREETT 281
#define T_RABIN 282
#define T_MULLER 283
#define T_ID 284
#define T_INT 285
#define T_BYTE 286
#define T_NAT 287
#define T_WIDE_ARROW 288
#define T_ASSIGNMENT 289
#define T_IMPLY 290
#define T_BOOL_AND 291
#define T_BOOL_OR 292
#define T_XOR 293
#define T_AND 294
#define T_OR 295
#define T_NEQ 296
#define T_EQ 297
#define T_GT 298
#define T_GEQ 299
#define T_LEQ 300
#define T_LT 301
#define T_RSHIFT 302
#define T_LSHIFT 303
#define T_PLUS 304
#define T_MINUS 305
#define T_MOD 306
#define T_DIV 307
#define T_MULT 308
#define UOPERATOR 309
#define T_DECREMENT 310
#define T_INCREMENT 311
#define T_UNARY_MINUS 312
#define T_BOOL_NOT 313
#define T_TILDE 314
#define T_ARROW 315
#define T_DOT 316
#define T_SQUARE_BRACKETS 317
#define T_PARENTHESIS 318




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 103 "dve_grammar.yy"
typedef union YYSTYPE {
  bool flag;
  int number;
  char string[MAXLEN];
} YYSTYPE;
/* Line 1318 of yacc.c.  */
#line 169 "dve_grammar.tab.hh"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE dve_yylval;

#if ! defined (YYLTYPE) && ! defined (YYLTYPE_IS_DECLARED)
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



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

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 1

/* Substitute the variable and function names.  */
#define yyparse dve_yyparse
#define yylex   dve_yylex
#define yyerror dve_yyerror
#define yylval  dve_yylval
#define yychar  dve_yychar
#define yydebug dve_yydebug
#define yynerrs dve_yynerrs
#define yylloc dve_yylloc

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




/* Copy the first part of user declarations.  */
#line 5 "dve_grammar.yy"

  #ifdef yylex
   #undef yylex
  #endif
  #include <iostream>
  #include "system/dve/syntax_analysis/dve_flex_lexer.hh"
  #include "system/dve/syntax_analysis/dve_grammar.hh"
  #include "common/error.hh"
//  #define YY_DECL int lexer_flex(void)
//  #define yylex lexer_flex
  #define YY_DECL static yyFlexLexer mylexer;
  #define YYINITDEPTH 30000
  #define YYMAXDEPTH 30000
  #ifdef yylex
   #undef yylex
  #endif
  #define yylex mylexer.yylex
  using namespace divine;
  using namespace std;

  YY_DECL;
  
  static dve_parser_t * parser;
  static error_vector_t * pterr;
  
  #define CALL(p1,p2,method) parser->set_fpos(p1.first_line,p1.first_column); parser->set_lpos(p2.last_line,p2.last_column); parser->method;
  #define PERROR(mes) (*pterr) << last_loc.first_line << ":" << last_loc.first_column << "-" << last_loc.last_line << ":" << last_loc.last_column << "  " << mes.message << thr(mes.type,mes.id)
  
  const ERR_type_t ERRTYPE = 351;
  
  ERR_c_triplet_t PE_VARDECL("Invalid variable declaration",ERRTYPE,34000);
  ERR_c_triplet_t PE_LOCALDECL("Error in local declarations",ERRTYPE,34001);
  ERR_c_triplet_t PE_EXPR("Error in local declarations",ERRTYPE,34002);
  ERR_c_triplet_t PE_VARINIT("Error in a variable initialization",ERRTYPE,34003);
  ERR_c_triplet_t PE_EFFECT_LIST("Error in declaration of list of effects",
                                 ERRTYPE,34004);
  ERR_c_triplet_t PE_PROCESS_CONTEXT("Error during parsing of expression"
                                     " inside a process context",
                                 ERRTYPE, 34005);
  ERR_c_triplet_t PE_ASSERT("Error in an assertion definition",ERRTYPE,34003);
  
  static dve_position_t last_loc;


/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 103 "dve_grammar.yy"
typedef union YYSTYPE {
  bool flag;
  int number;
  char string[MAXLEN];
} YYSTYPE;
/* Line 190 of yacc.c.  */
#line 260 "dve_grammar.tab.cc"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

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


/* Copy the second part of user declarations.  */


/* Line 213 of yacc.c.  */
#line 284 "dve_grammar.tab.cc"

#if ! defined (yyoverflow) || YYERROR_VERBOSE

# ifndef YYFREE
#  define YYFREE free
# endif
# ifndef YYMALLOC
#  define YYMALLOC malloc
# endif

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   else
#    define YYSTACK_ALLOC alloca
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (defined (YYLTYPE_IS_TRIVIAL) && YYLTYPE_IS_TRIVIAL \
             && defined (YYSTYPE_IS_TRIVIAL) && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short int yyss;
  YYSTYPE yyvs;
    YYLTYPE yyls;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short int) + sizeof (YYSTYPE) + sizeof (YYLTYPE))	\
      + 2 * YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined (__GNUC__) && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short int yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   529

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  75
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  82
/* YYNRULES -- Number of rules. */
#define YYNRULES  174
/* YYNRULES -- Number of states. */
#define YYNSTATES  307

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   318

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      60,    61,     2,     2,    70,     2,    64,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    73,    69,
       2,     2,     2,    74,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    62,     2,    63,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    71,     2,    72,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    65,    66,    67,    68
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short int yyprhs[] =
{
       0,     0,     3,     7,     8,    11,    14,    16,    17,    22,
      23,    28,    29,    31,    34,    37,    39,    41,    43,    47,
      51,    52,    53,    57,    60,    62,    66,    68,    72,    74,
      75,    79,    82,    84,    85,    92,    98,    99,   102,   105,
     109,   116,   118,   122,   124,   126,   130,   135,   137,   141,
     143,   147,   149,   153,   155,   159,   161,   164,   167,   170,
     173,   175,   177,   180,   184,   188,   192,   196,   200,   203,
     207,   210,   214,   218,   221,   223,   227,   230,   234,   237,
     240,   244,   246,   249,   251,   255,   258,   261,   265,   266,
     270,   272,   276,   277,   282,   286,   287,   291,   293,   297,
     306,   308,   314,   318,   320,   324,   332,   334,   335,   336,
     341,   342,   347,   348,   352,   353,   358,   359,   365,   366,
     371,   372,   378,   379,   381,   385,   387,   391,   393,   394,
     395,   396,   402,   405,   407,   411,   413,   415,   417,   419,
     424,   428,   431,   435,   439,   443,   447,   451,   455,   459,
     463,   467,   471,   475,   479,   483,   487,   491,   495,   499,
     503,   507,   511,   518,   522,   526,   530,   532,   534,   536,
     538,   541,   545,   549,   550
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short int yyrhs[] =
{
      76,     0,    -1,    77,    93,   154,    -1,    -1,    77,    79,
      -1,    77,    98,    -1,    29,    -1,    -1,    83,    80,    85,
      69,    -1,    -1,    83,     1,    81,    69,    -1,    -1,     7,
      -1,    82,    84,    -1,     7,     1,    -1,    30,    -1,    31,
      -1,    86,    -1,    85,    70,    86,    -1,    78,    92,    87,
      -1,    -1,    -1,    34,    88,    89,    -1,    34,     1,    -1,
     150,    -1,    71,    90,    72,    -1,    91,    -1,    90,    70,
      91,    -1,   150,    -1,    -1,    62,    32,    63,    -1,    94,
      93,    -1,    94,    -1,    -1,    11,    78,    95,    71,    96,
      72,    -1,    97,   105,   109,   123,   127,    -1,    -1,    97,
      79,    -1,    97,     1,    -1,    13,    99,    69,    -1,    13,
      71,   103,    72,   101,    69,    -1,   100,    -1,    99,    70,
     100,    -1,    78,    -1,   102,    -1,   101,    70,   102,    -1,
      78,    62,    32,    63,    -1,   104,    -1,   103,    70,   104,
      -1,    84,    -1,    12,   106,    69,    -1,   107,    -1,   106,
      70,   107,    -1,    78,    -1,    15,    78,    69,    -1,   108,
      -1,   108,   110,    -1,   110,   108,    -1,   121,   111,    -1,
     111,   121,    -1,   111,    -1,   121,    -1,    16,   112,    -1,
      16,    24,   112,    -1,    16,    25,   113,    -1,    16,    28,
     113,    -1,    16,    27,   116,    -1,    16,    26,   116,    -1,
      78,    69,    -1,    78,    70,   112,    -1,   114,    69,    -1,
     114,    70,   113,    -1,    60,   115,    61,    -1,    60,    61,
      -1,    78,    -1,    78,    70,   115,    -1,   117,    69,    -1,
     117,    70,   116,    -1,   118,   119,    -1,    60,    69,    -1,
      60,   120,    69,    -1,    61,    -1,   120,    61,    -1,    78,
      -1,    78,    70,   120,    -1,    14,   122,    -1,    78,    69,
      -1,    78,    70,   122,    -1,    -1,    17,   124,    69,    -1,
     125,    -1,   124,    70,   125,    -1,    -1,    78,    73,   126,
     150,    -1,    78,    73,     1,    -1,    -1,    18,   128,    69,
      -1,   129,    -1,   128,    70,   133,    -1,    78,    65,    78,
      71,   134,   137,   146,    72,    -1,   130,    -1,    78,    33,
      71,   132,    72,    -1,    78,    73,    32,    -1,   131,    -1,
     132,    70,   131,    -1,    65,    78,    71,   134,   137,   146,
      72,    -1,   129,    -1,    -1,    -1,    19,   135,   150,    69,
      -1,    -1,    19,     1,   136,    69,    -1,    -1,    20,   138,
      69,    -1,    -1,    78,     6,   139,   143,    -1,    -1,    78,
       6,     1,   140,    69,    -1,    -1,    78,    74,   141,   143,
      -1,    -1,    78,    74,     1,   142,    69,    -1,    -1,   150,
      -1,    71,   144,    72,    -1,   145,    -1,   144,    70,   145,
      -1,   150,    -1,    -1,    -1,    -1,    23,   147,   149,    69,
     148,    -1,    23,     1,    -1,   151,    -1,   149,    70,   151,
      -1,     9,    -1,     8,    -1,    32,    -1,    78,    -1,    78,
      62,   150,    63,    -1,    60,   150,    61,    -1,   153,   150,
      -1,   150,    46,   150,    -1,   150,    45,   150,    -1,   150,
      42,   150,    -1,   150,    41,   150,    -1,   150,    43,   150,
      -1,   150,    44,   150,    -1,   150,    49,   150,    -1,   150,
      50,   150,    -1,   150,    53,   150,    -1,   150,    52,   150,
      -1,   150,    51,   150,    -1,   150,    39,   150,    -1,   150,
      40,   150,    -1,   150,    38,   150,    -1,   150,    48,   150,
      -1,   150,    47,   150,    -1,   150,    36,   150,    -1,   150,
      37,   150,    -1,    78,    64,    78,    -1,    78,    65,    78,
      -1,    78,    65,    78,    62,   150,    63,    -1,   150,    35,
     150,    -1,   150,   152,   150,    -1,   150,   152,     1,    -1,
      34,    -1,    50,    -1,    59,    -1,    58,    -1,    10,   155,
      -1,    21,   156,    69,    -1,    20,   156,    69,    -1,    -1,
      22,    78,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   120,   120,   123,   125,   126,   131,   137,   137,   139,
     139,   143,   144,   148,   149,   153,   154,   159,   160,   164,
     168,   169,   169,   170,   174,   175,   179,   180,   184,   188,
     189,   198,   199,   209,   209,   217,   221,   223,   224,   231,
     232,   236,   237,   241,   245,   246,   250,   258,   259,   263,
     271,   275,   276,   280,   287,   291,   292,   293,   297,   298,
     299,   300,   304,   305,   306,   307,   308,   309,   313,   314,
     318,   319,   323,   324,   328,   329,   333,   334,   338,   342,
     343,   347,   348,   352,   353,   357,   361,   362,   369,   371,
     375,   376,   380,   380,   381,   387,   389,   393,   394,   403,
     405,   409,   413,   417,   418,   422,   424,   428,   429,   429,
     431,   431,   435,   436,   440,   440,   442,   442,   443,   443,
     445,   445,   449,   450,   451,   455,   456,   460,   464,   465,
     465,   465,   467,   474,   475,   490,   492,   494,   496,   498,
     500,   502,   504,   506,   508,   510,   512,   514,   516,   518,
     520,   522,   524,   526,   528,   530,   532,   534,   536,   538,
     540,   544,   546,   548,   555,   557,   562,   567,   568,   569,
     575,   579,   581,   586,   587
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "T_NO_TOKEN", "T_FOREIGN_ID",
  "T_FOREIGN_SQUARE_BRACKETS", "T_EXCLAM", "T_CONST", "T_TRUE", "T_FALSE",
  "T_SYSTEM", "T_PROCESS", "T_STATE", "T_CHANNEL", "T_COMMIT", "T_INIT",
  "T_ACCEPT", "T_ASSERT", "T_TRANS", "T_GUARD", "T_SYNC", "T_ASYNC",
  "T_PROPERTY", "T_EFFECT", "T_BUCHI", "T_GENBUCHI", "T_STREETT",
  "T_RABIN", "T_MULLER", "T_ID", "T_INT", "T_BYTE", "T_NAT",
  "T_WIDE_ARROW", "T_ASSIGNMENT", "T_IMPLY", "T_BOOL_AND", "T_BOOL_OR",
  "T_XOR", "T_AND", "T_OR", "T_NEQ", "T_EQ", "T_GT", "T_GEQ", "T_LEQ",
  "T_LT", "T_RSHIFT", "T_LSHIFT", "T_PLUS", "T_MINUS", "T_MOD", "T_DIV",
  "T_MULT", "UOPERATOR", "T_DECREMENT", "T_INCREMENT", "T_UNARY_MINUS",
  "T_BOOL_NOT", "T_TILDE", "'('", "')'", "'['", "']'", "'.'", "T_ARROW",
  "T_DOT", "T_SQUARE_BRACKETS", "T_PARENTHESIS", "';'", "','", "'{'",
  "'}'", "':'", "'?'", "$accept", "DVE", "Declaration", "Id",
  "VariableDecl", "@1", "@2", "Const", "TypeName", "TypeId", "DeclIdList",
  "DeclId", "VarInit", "@3", "Initializer", "FieldInitList", "FieldInit",
  "ArrayDecl", "ProcDeclList", "ProcDecl", "@4", "ProcBody",
  "ProcLocalDeclList", "Channels", "ChannelDeclList", "ChannelDecl",
  "TypedChannelDeclList", "TypedChannelDecl", "TypeList", "OneTypeId",
  "States", "StateDeclList", "StateDecl", "Init", "InitAndCommitAndAccept",
  "CommitAndAccept", "Accept", "AcceptListBuchi",
  "AcceptListGenBuchiMuller", "AcceptListGenBuchiMullerSet",
  "AcceptListGenBuchiMullerSetNotEmpty", "AcceptListRabinStreett",
  "AcceptListRabinStreettPair", "AcceptListRabinStreettFirst",
  "AcceptListRabinStreettSecond", "AcceptListRabinStreettSet", "Commit",
  "CommitList", "Assertions", "AssertionList", "Assertion", "@5",
  "Transitions", "TransitionList", "Transition", "ProbTransition",
  "ProbTransitionPart", "ProbList", "TransitionOpt", "Guard", "@6", "@7",
  "Sync", "SyncExpr", "@8", "@9", "@10", "@11", "SyncValue",
  "ExpressionList", "OneExpression", "Effect", "@12", "@13", "EffList",
  "Expression", "Assignment", "AssignOp", "UnaryOp", "System",
  "SystemType", "ProcProperty", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short int yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
      40,    41,    91,    93,    46,   315,   316,   317,   318,    59,
      44,   123,   125,    58,    63
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    75,    76,    77,    77,    77,    78,    80,    79,    81,
      79,    82,    82,    83,    83,    84,    84,    85,    85,    86,
      87,    88,    87,    87,    89,    89,    90,    90,    91,    92,
      92,    93,    93,    95,    94,    96,    97,    97,    97,    98,
      98,    99,    99,   100,   101,   101,   102,   103,   103,   104,
     105,   106,   106,   107,   108,   109,   109,   109,   110,   110,
     110,   110,   111,   111,   111,   111,   111,   111,   112,   112,
     113,   113,   114,   114,   115,   115,   116,   116,   117,   118,
     118,   119,   119,   120,   120,   121,   122,   122,   123,   123,
     124,   124,   126,   125,   125,   127,   127,   128,   128,   129,
     129,   130,   131,   132,   132,   133,   133,   134,   135,   134,
     136,   134,   137,   137,   139,   138,   140,   138,   141,   138,
     142,   138,   143,   143,   143,   144,   144,   145,   146,   147,
     148,   146,   146,   149,   149,   150,   150,   150,   150,   150,
     150,   150,   150,   150,   150,   150,   150,   150,   150,   150,
     150,   150,   150,   150,   150,   150,   150,   150,   150,   150,
     150,   150,   150,   150,   151,   151,   152,   153,   153,   153,
     154,   155,   155,   156,   156
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     3,     0,     2,     2,     1,     0,     4,     0,
       4,     0,     1,     2,     2,     1,     1,     1,     3,     3,
       0,     0,     3,     2,     1,     3,     1,     3,     1,     0,
       3,     2,     1,     0,     6,     5,     0,     2,     2,     3,
       6,     1,     3,     1,     1,     3,     4,     1,     3,     1,
       3,     1,     3,     1,     3,     1,     2,     2,     2,     2,
       1,     1,     2,     3,     3,     3,     3,     3,     2,     3,
       2,     3,     3,     2,     1,     3,     2,     3,     2,     2,
       3,     1,     2,     1,     3,     2,     2,     3,     0,     3,
       1,     3,     0,     4,     3,     0,     3,     1,     3,     8,
       1,     5,     3,     1,     3,     7,     1,     0,     0,     4,
       0,     4,     0,     3,     0,     4,     0,     5,     0,     4,
       0,     5,     0,     1,     3,     1,     3,     1,     0,     0,
       0,     5,     2,     1,     3,     1,     1,     1,     1,     4,
       3,     2,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     6,     3,     3,     3,     1,     1,     1,     1,
       2,     3,     3,     0,     2
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       3,     0,    11,     1,     0,     0,     0,     4,     0,     0,
       0,    32,     5,    14,     6,    33,     0,    43,     0,    41,
      15,    16,    13,     9,     0,     0,     2,    31,     0,    49,
       0,    47,    39,     0,     0,    29,     0,    17,   173,   173,
     170,    36,     0,     0,    42,    10,     0,    20,     8,     0,
       0,     0,     0,     0,     0,    48,     0,     0,    44,     0,
       0,    19,    18,   174,   172,   171,    34,    38,     0,    37,
       0,     0,    40,     0,    30,    23,     0,    53,     0,    51,
       0,     0,     0,    55,    88,     0,    60,    61,     0,    45,
     136,   135,   137,   167,   169,   168,     0,     0,   138,    22,
      24,     0,    50,     0,     0,    85,     0,     0,     0,     0,
       0,     0,     0,    62,    56,     0,    95,    57,    59,    58,
      46,     0,     0,    26,    28,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   141,    52,    86,
       0,    54,    63,     0,    64,     0,     0,    67,     0,     0,
      66,    65,    68,     0,     0,     0,    90,     0,    35,   140,
       0,    25,     0,   160,   161,   163,   158,   159,   155,   153,
     154,   145,   144,   146,   147,   143,   142,   157,   156,   148,
     149,   152,   151,   150,    87,    73,    74,     0,    70,     0,
      79,    83,     0,    76,     0,    81,    78,     0,    69,     0,
      89,     0,     0,     0,    97,   100,    27,   139,     0,     0,
      72,    71,     0,    80,    77,    82,    94,     0,    91,     0,
       0,    96,     0,     0,    75,    84,    93,     0,     0,     0,
     106,    98,   162,     0,   103,     0,   107,     0,     0,     0,
     101,     0,   112,   107,   102,   104,   110,     0,     0,   128,
     112,     0,     0,     0,     0,     0,     0,   128,   111,   109,
       0,     0,   113,   132,     0,    99,     0,   116,   122,   120,
     122,     0,     0,   133,   105,     0,     0,   115,   123,     0,
     119,   130,     0,   166,     0,   117,     0,   125,   127,   121,
     131,   134,   165,   164,     0,   124,   126
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,     1,     2,    98,     7,    24,    34,     8,     9,    29,
      36,    37,    61,    76,    99,   122,   123,    47,    10,    11,
      28,    53,    54,    12,    18,    19,    57,    58,    30,    31,
      70,    78,    79,    83,    84,    85,    86,   113,   154,   155,
     197,   157,   158,   159,   206,   202,    87,   105,   116,   165,
     166,   227,   168,   213,   214,   215,   244,   245,   241,   252,
     257,   261,   259,   264,   278,   285,   280,   289,   287,   296,
     297,   266,   274,   300,   281,   124,   283,   294,   101,    26,
      40,    51
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -154
static const short int yypact[] =
{
    -154,    17,    13,  -154,    78,    -8,   -24,  -154,   119,    12,
      20,    16,  -154,  -154,  -154,  -154,   119,  -154,    50,  -154,
    -154,  -154,  -154,  -154,    -8,   132,  -154,  -154,   -34,  -154,
      31,  -154,  -154,    -8,   -27,    10,   114,  -154,    29,    29,
    -154,  -154,   119,    -8,  -154,  -154,    49,    30,  -154,    -8,
      -8,    25,    42,    55,    28,  -154,    45,   139,  -154,    41,
       7,  -154,  -154,  -154,  -154,  -154,  -154,  -154,    -8,  -154,
     109,    99,  -154,    -8,  -154,  -154,   178,  -154,   142,  -154,
      -8,    -8,    89,    32,   143,   144,   149,   169,   150,  -154,
    -154,  -154,  -154,  -154,  -154,  -154,    97,    97,    35,  -154,
     449,    97,  -154,    -8,   151,  -154,   147,    -8,   164,   166,
     166,   164,   160,  -154,  -154,    -8,   213,  -154,  -154,  -154,
    -154,   402,    58,  -154,   449,    97,    -8,    -8,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,  -154,  -154,  -154,
      -8,  -154,  -154,   -11,  -154,   171,   -20,  -154,   181,    -7,
    -154,  -154,  -154,    -8,   162,   188,  -154,    -8,  -154,  -154,
      97,  -154,   344,  -154,   183,   152,   465,   465,   322,   322,
     322,   476,   476,    38,    38,    38,    38,    90,    90,    82,
      82,  -154,  -154,  -154,  -154,  -154,   163,   186,  -154,   164,
    -154,   182,   190,  -154,   166,  -154,  -154,   206,  -154,   214,
    -154,    -8,   -10,   192,  -154,  -154,  -154,  -154,    97,    -8,
    -154,  -154,    -8,  -154,  -154,  -154,  -154,    97,  -154,   200,
      -8,  -154,   -22,   373,  -154,  -154,   449,    -8,   204,    -8,
    -154,  -154,  -154,   205,  -154,    66,   258,   211,   251,    -8,
    -154,   247,   265,   258,  -154,  -154,  -154,    97,    -8,   263,
     265,   220,   309,     6,   221,   279,   222,   263,  -154,  -154,
       2,    24,  -154,  -154,    97,  -154,   223,  -154,   210,  -154,
     210,   196,   430,  -154,  -154,   227,    97,  -154,   449,   229,
    -154,  -154,    97,  -154,   283,  -154,    74,  -154,   449,  -154,
    -154,  -154,  -154,   449,    97,  -154,  -154
};

/* YYPGOTO[NTERM-NUM].  */
static const short int yypgoto[] =
{
    -154,  -154,  -154,    -5,   245,  -154,  -154,  -154,  -154,   292,
    -154,   252,  -154,  -154,  -154,  -154,   133,  -154,   291,  -154,
    -154,  -154,  -154,  -154,  -154,   271,  -154,   236,  -154,   275,
    -154,  -154,   215,   228,  -154,   237,   232,   -93,  -107,  -154,
     102,  -108,  -154,  -154,  -154,  -153,   238,   173,  -154,  -154,
     115,  -154,  -154,  -154,    93,  -154,    83,  -154,  -154,    81,
    -154,  -154,    67,  -154,  -154,  -154,  -154,  -154,    51,  -154,
      72,    68,  -154,  -154,  -154,    36,    44,  -154,  -154,  -154,
    -154,   338
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -130
static const short int yytable[] =
{
      15,    17,   160,   277,   161,    14,   207,    14,    75,    14,
    -114,  -114,   270,    23,   152,   -21,   -21,     3,    14,    35,
       4,    14,    14,   229,     5,   279,     6,     5,    17,    67,
      25,  -114,  -118,  -118,  -114,     4,   -21,    41,    56,   -21,
      68,    -7,    45,   239,    35,    63,    80,    16,    82,   200,
     195,    50,  -114,  -118,   205,   230,  -118,   -21,   -11,   -11,
    -114,  -114,  -114,    77,    60,   -21,   -21,   -21,    56,   235,
     208,  -114,    46,  -114,  -118,   104,   106,   112,   -21,    13,
     271,    59,  -118,  -118,  -118,   140,   141,   142,   143,   144,
     145,   146,   221,  -118,    64,  -118,   224,   125,    77,   126,
     127,    42,   112,    43,    74,    90,    91,    71,   -12,   -12,
     164,    65,   100,   107,   108,   109,   110,   111,    14,    32,
      33,   173,   174,    80,    81,    82,    14,    66,   170,    92,
     171,    88,   121,   144,   145,   146,   249,   147,   250,   142,
     143,   144,   145,   146,   304,   104,   305,    93,   196,    20,
      21,   201,    38,    39,   201,    94,    95,    96,   112,    81,
     115,   172,   212,    80,   175,   176,   177,   178,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   188,   189,   190,
     191,   192,   193,    48,    49,    82,    90,    91,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   164,    14,    72,    73,
      92,   102,   103,   120,   196,   226,   151,   201,    90,    91,
     149,   150,   -92,   -92,   153,   238,   156,   212,    93,   162,
     163,   167,   243,   219,   247,   209,    94,    95,    96,    14,
     198,   199,    92,   -92,   243,   218,   -92,   220,   256,    97,
     203,   204,   222,   263,   233,  -108,  -108,   210,   211,   223,
      93,   231,   232,   236,   -92,   291,   292,   225,    94,    95,
      96,   237,   -92,   -92,   -92,   246,  -108,   251,   248,  -108,
     273,   286,   253,   254,   302,   258,   265,  -129,  -129,   268,
     272,    90,    91,   262,   275,   284,   295,  -108,   299,    69,
      22,    62,    27,   216,    44,  -108,  -108,  -108,  -129,    89,
     282,  -129,    14,   117,   288,    92,   288,    55,   148,   119,
     114,   234,   298,   194,   118,   240,   228,   267,   282,  -129,
     303,   290,   255,    93,   260,   276,   301,  -129,  -129,  -129,
     298,    94,    95,    96,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   306,    52,   269,   128,
     129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   217,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   146,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   242,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   143,   144,   145,   146,     0,     0,     0,     0,
       0,     0,     0,   169,   293,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145,   146
};

static const short int yycheck[] =
{
       5,     6,   110,     1,   111,    29,   159,    29,     1,    29,
       8,     9,     6,     1,   107,     8,     9,     0,    29,    24,
       7,    29,    29,    33,    11,     1,    13,    11,    33,     1,
      10,    29,     8,     9,    32,     7,    29,    71,    43,    32,
      12,    29,    69,    65,    49,    50,    14,    71,    16,    69,
      61,    22,    50,    29,    61,    65,    32,    50,    30,    31,
      58,    59,    60,    68,    34,    58,    59,    60,    73,   222,
     163,    69,    62,    71,    50,    80,    81,    82,    71,     1,
      74,    32,    58,    59,    60,    47,    48,    49,    50,    51,
      52,    53,   199,    69,    69,    71,   204,    62,   103,    64,
      65,    70,   107,    72,    63,     8,     9,    62,    30,    31,
     115,    69,    76,    24,    25,    26,    27,    28,    29,    69,
      70,   126,   127,    14,    15,    16,    29,    72,    70,    32,
      72,    32,    96,    51,    52,    53,    70,   101,    72,    49,
      50,    51,    52,    53,    70,   150,    72,    50,   153,    30,
      31,   156,    20,    21,   159,    58,    59,    60,   163,    15,
      17,   125,   167,    14,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,    69,    70,    16,     8,     9,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,   211,    29,    69,    70,
      32,    69,    70,    63,   219,     1,    69,   222,     8,     9,
      69,    70,     8,     9,    60,   230,    60,   232,    50,    69,
      70,    18,   237,    70,   239,    73,    58,    59,    60,    29,
      69,    70,    32,    29,   249,    62,    32,    61,     1,    71,
      69,    70,    70,   258,   218,     8,     9,    69,    70,    69,
      50,    69,    70,   227,    50,    69,    70,    61,    58,    59,
      60,    71,    58,    59,    60,    71,    29,    19,    73,    32,
       1,    71,    71,    32,     1,    20,    23,     8,     9,    69,
      69,     8,     9,   257,    72,    72,    69,    50,    69,    54,
       8,    49,    11,   170,    33,    58,    59,    60,    29,    73,
     274,    32,    29,    85,   278,    32,   280,    42,   103,    87,
      83,   219,   286,   150,    86,   232,   211,   260,   292,    50,
     294,   280,   249,    50,   253,   267,   292,    58,    59,    60,
     304,    58,    59,    60,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,   304,    39,    69,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    63,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    63,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    61,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,    76,    77,     0,     7,    11,    13,    79,    82,    83,
      93,    94,    98,     1,    29,    78,    71,    78,    99,   100,
      30,    31,    84,     1,    80,    10,   154,    93,    95,    84,
     103,   104,    69,    70,    81,    78,    85,    86,    20,    21,
     155,    71,    70,    72,   100,    69,    62,    92,    69,    70,
      22,   156,   156,    96,    97,   104,    78,   101,   102,    32,
      34,    87,    86,    78,    69,    69,    72,     1,    12,    79,
     105,    62,    69,    70,    63,     1,    88,    78,   106,   107,
      14,    15,    16,   108,   109,   110,   111,   121,    32,   102,
       8,     9,    32,    50,    58,    59,    60,    71,    78,    89,
     150,   153,    69,    70,    78,   122,    78,    24,    25,    26,
      27,    28,    78,   112,   110,    17,   123,   108,   121,   111,
      63,   150,    90,    91,   150,    62,    64,    65,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,   150,   107,    69,
      70,    69,   112,    60,   113,   114,    60,   116,   117,   118,
     116,   113,    69,    70,    78,   124,   125,    18,   127,    61,
      70,    72,   150,    78,    78,   150,   150,   150,   150,   150,
     150,   150,   150,   150,   150,   150,   150,   150,   150,   150,
     150,   150,   150,   150,   122,    61,    78,   115,    69,    70,
      69,    78,   120,    69,    70,    61,   119,   120,   112,    73,
      69,    70,    78,   128,   129,   130,    91,    63,    62,    70,
      61,   113,    70,    69,   116,    61,     1,   126,   125,    33,
      65,    69,    70,   150,   115,   120,   150,    71,    78,    65,
     129,   133,    63,    78,   131,   132,    71,    78,    73,    70,
      72,    19,   134,    71,    32,   131,     1,   135,    20,   137,
     134,   136,   150,    78,   138,    23,   146,   137,    69,    69,
       6,    74,    69,     1,   147,    72,   146,     1,   139,     1,
     141,   149,   150,   151,    72,   140,    71,   143,   150,   142,
     143,    69,    70,    34,   152,    69,   144,   145,   150,    69,
     148,   151,     1,   150,    70,    72,   145
};

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");\
      YYERROR;							\
    }								\
while (0)


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (N)								\
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (0)
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
              (Loc).first_line, (Loc).first_column,	\
              (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr, 					\
                  Type, Value, Location);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short int *bottom, short int *top)
#else
static void
yy_stack_print (bottom, top)
    short int *bottom;
    short int *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %u), ",
             yyrule - 1, yylno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname [yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname [yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

#endif /* !YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep, yylocationp)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;
  (void) yylocationp;

  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  fprintf (yyoutput, ": ");

# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yylocationp)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;
  (void) yylocationp;

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
        break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;
/* Location data for the look-ahead symbol.  */
YYLTYPE yylloc;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short int yyssa[YYINITDEPTH];
  short int *yyss = yyssa;
  register short int *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;

  /* The location stack.  */
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
  /* The locations where the error started and ended. */
  YYLTYPE yyerror_range[2];

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;
  yylsp = yyls;
#if YYLTYPE_IS_TRIVIAL
  /* Initialize the default location before parsing starts.  */
  yylloc.first_line   = yylloc.last_line   = 1;
  yylloc.first_column = yylloc.last_column = 0;
#endif


  yyvsp[0] = yylval;
    yylsp[0] = yylloc;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short int *yyss1 = yyss;
	YYLTYPE *yyls1 = yyls;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);
	yyls = yyls1;
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short int *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);
	YYSTACK_RELOCATE (yyls);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a look-ahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to look-ahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
  *++yylsp = yylloc;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, yylsp - yylen, yylen);
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 120 "dve_grammar.yy"
    { parser->done(); ;}
    break;

  case 6:
#line 131 "dve_grammar.yy"
    { strncpy((yyval.string), (yyvsp[0].string), MAXLEN); ;}
    break;

  case 7:
#line 137 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),var_decl_begin((yyvsp[0].number))); ;}
    break;

  case 8:
#line 138 "dve_grammar.yy"
    { CALL((yylsp[-3]),(yylsp[0]),var_decl_done()); ;}
    break;

  case 9:
#line 139 "dve_grammar.yy"
    { PERROR(PE_VARDECL); CALL((yylsp[-1]),(yylsp[-1]),var_decl_cancel()); ;}
    break;

  case 11:
#line 143 "dve_grammar.yy"
    { (yyval.number) = false; ;}
    break;

  case 12:
#line 144 "dve_grammar.yy"
    { (yyval.number) = true; ;}
    break;

  case 13:
#line 148 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),type_is_const((yyvsp[-1].number))); (yyval.number) = (yyvsp[0].number); ;}
    break;

  case 14:
#line 149 "dve_grammar.yy"
    { PERROR(PE_VARDECL); ;}
    break;

  case 15:
#line 153 "dve_grammar.yy"
    { (yyval.number) = T_INT; ;}
    break;

  case 16:
#line 154 "dve_grammar.yy"
    { (yyval.number) = T_BYTE; ;}
    break;

  case 19:
#line 164 "dve_grammar.yy"
    { CALL((yylsp[-2]), (yylsp[0]), var_decl_create((yyvsp[-2].string), (yyvsp[-1].number), (yyvsp[0].flag))); ;}
    break;

  case 20:
#line 168 "dve_grammar.yy"
    { (yyval.flag) = false; ;}
    break;

  case 21:
#line 169 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),take_expression()); ;}
    break;

  case 22:
#line 169 "dve_grammar.yy"
    { (yyval.flag) = true; ;}
    break;

  case 23:
#line 170 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),take_expression_cancel()); PERROR(PE_VARINIT); ;}
    break;

  case 24:
#line 174 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),var_init_is_field(false)); ;}
    break;

  case 25:
#line 175 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),var_init_is_field(true)); ;}
    break;

  case 26:
#line 179 "dve_grammar.yy"
    {;}
    break;

  case 27:
#line 180 "dve_grammar.yy"
    {;}
    break;

  case 28:
#line 184 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),var_init_field_part()); ;}
    break;

  case 29:
#line 188 "dve_grammar.yy"
    { (yyval.number) = 0; ;}
    break;

  case 30:
#line 189 "dve_grammar.yy"
    { (yyval.number) = 1; CALL((yylsp[-2]),(yylsp[0]),var_decl_array_size((yyvsp[-1].number))); ;}
    break;

  case 33:
#line 209 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[0]),proc_decl_begin((yyvsp[0].string))); ;}
    break;

  case 34:
#line 210 "dve_grammar.yy"
    { CALL((yylsp[-5]),(yylsp[0]),proc_decl_done()); ;}
    break;

  case 37:
#line 223 "dve_grammar.yy"
    {;}
    break;

  case 38:
#line 224 "dve_grammar.yy"
    { PERROR(PE_LOCALDECL); ;}
    break;

  case 40:
#line 232 "dve_grammar.yy"
    { CALL((yylsp[-5]),(yylsp[-1]),type_list_clear()); ;}
    break;

  case 43:
#line 241 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),channel_decl((yyvsp[0].string))); ;}
    break;

  case 46:
#line 250 "dve_grammar.yy"
    { CALL((yylsp[-3]),(yylsp[0]),typed_channel_decl((yyvsp[-3].string),(yyvsp[-1].number))); ;}
    break;

  case 49:
#line 263 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),type_list_add((yyvsp[0].number))); ;}
    break;

  case 50:
#line 271 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),state_list_done()); ;}
    break;

  case 53:
#line 280 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),state_decl((yyvsp[0].string))); ;}
    break;

  case 54:
#line 287 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),state_init((yyvsp[-1].string))); ;}
    break;

  case 62:
#line 304 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[0]),accept_type(T_BUCHI)); ;}
    break;

  case 63:
#line 305 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),accept_type(T_BUCHI)); ;}
    break;

  case 64:
#line 306 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),accept_type(T_GENBUCHI)); ;}
    break;

  case 65:
#line 307 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),accept_type(T_MULLER)); ;}
    break;

  case 66:
#line 308 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),accept_type(T_RABIN)); ;}
    break;

  case 67:
#line 309 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),accept_type(T_STREETT)); ;}
    break;

  case 68:
#line 313 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),state_accept((yyvsp[-1].string))); ;}
    break;

  case 69:
#line 314 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),state_accept((yyvsp[-2].string))); ;}
    break;

  case 70:
#line 318 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),accept_genbuchi_muller_set_complete()); ;}
    break;

  case 71:
#line 319 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),accept_genbuchi_muller_set_complete()); ;}
    break;

  case 74:
#line 328 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),state_genbuchi_muller_accept((yyvsp[0].string))); ;}
    break;

  case 75:
#line 329 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),state_genbuchi_muller_accept((yyvsp[-2].string))); ;}
    break;

  case 76:
#line 333 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),accept_rabin_streett_pair_complete()); ;}
    break;

  case 77:
#line 334 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),accept_rabin_streett_pair_complete()); ;}
    break;

  case 79:
#line 342 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[0]),accept_rabin_streett_first_complete()); ;}
    break;

  case 80:
#line 343 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),accept_rabin_streett_first_complete()); ;}
    break;

  case 83:
#line 352 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),state_rabin_streett_accept((yyvsp[0].string))); ;}
    break;

  case 84:
#line 353 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),state_rabin_streett_accept((yyvsp[-2].string))); ;}
    break;

  case 86:
#line 361 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),state_commit((yyvsp[-1].string))); ;}
    break;

  case 87:
#line 362 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),state_commit((yyvsp[-2].string))); ;}
    break;

  case 92:
#line 380 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),take_expression()); ;}
    break;

  case 93:
#line 380 "dve_grammar.yy"
    { CALL((yylsp[-3]),(yylsp[-1]),assertion_create((yyvsp[-3].string))); ;}
    break;

  case 94:
#line 381 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),take_expression_cancel()); PERROR(PE_ASSERT); ;}
    break;

  case 99:
#line 404 "dve_grammar.yy"
    { CALL((yylsp[-7]),(yylsp[0]),trans_create((yyvsp[-7].string),(yyvsp[-5].string),(yyvsp[-3].flag),(yyvsp[-2].number),(yyvsp[-1].number))); ;}
    break;

  case 101:
#line 409 "dve_grammar.yy"
    { CALL((yylsp[-4]),(yylsp[0]),prob_trans_create((yyvsp[-4].string))); ;}
    break;

  case 102:
#line 413 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),prob_transition_part((yyvsp[-2].string),(yyvsp[0].number))); ;}
    break;

  case 105:
#line 423 "dve_grammar.yy"
    { CALL((yylsp[-6]),(yylsp[0]),trans_create(0,(yyvsp[-5].string),(yyvsp[-3].flag),(yyvsp[-2].number),(yyvsp[-1].number))); ;}
    break;

  case 107:
#line 428 "dve_grammar.yy"
    { (yyval.flag) = false ;}
    break;

  case 108:
#line 429 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),take_expression()); ;}
    break;

  case 109:
#line 430 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),trans_guard_expr()); (yyval.flag) = true; ;}
    break;

  case 110:
#line 431 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),take_expression_cancel()) ;}
    break;

  case 111:
#line 431 "dve_grammar.yy"
    { (yyval.flag) = false; ;}
    break;

  case 112:
#line 435 "dve_grammar.yy"
    { (yyval.number) = 0; ;}
    break;

  case 113:
#line 436 "dve_grammar.yy"
    { (yyval.number) = (yyvsp[-1].number); ;}
    break;

  case 114:
#line 440 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[0]),take_expression()); ;}
    break;

  case 115:
#line 441 "dve_grammar.yy"
    { CALL((yylsp[-3]),(yylsp[0]),trans_sync((yyvsp[-3].string),1,(yyvsp[0].flag))); (yyval.number) = 1; ;}
    break;

  case 116:
#line 442 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),take_expression_cancel()); ;}
    break;

  case 117:
#line 442 "dve_grammar.yy"
    { (yyval.number) = 1; ;}
    break;

  case 118:
#line 443 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[0]),take_expression()); ;}
    break;

  case 119:
#line 444 "dve_grammar.yy"
    { CALL((yylsp[-3]),(yylsp[0]),trans_sync((yyvsp[-3].string),2,(yyvsp[0].flag))); (yyval.number) = 2; ;}
    break;

  case 120:
#line 445 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),take_expression_cancel()); ;}
    break;

  case 121:
#line 445 "dve_grammar.yy"
    { (yyval.number) = 1; ;}
    break;

  case 122:
#line 449 "dve_grammar.yy"
    { (yyval.flag) = false; ;}
    break;

  case 123:
#line 450 "dve_grammar.yy"
    { (yyval.flag) = true; ;}
    break;

  case 124:
#line 451 "dve_grammar.yy"
    { (yyval.flag) = true; ;}
    break;

  case 127:
#line 460 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]), expression_list_store()); ;}
    break;

  case 128:
#line 464 "dve_grammar.yy"
    { (yyval.number) = 0; ;}
    break;

  case 129:
#line 465 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),trans_effect_list_begin()); ;}
    break;

  case 130:
#line 465 "dve_grammar.yy"
    { CALL((yylsp[-3]),(yylsp[-3]),trans_effect_list_end()); ;}
    break;

  case 131:
#line 466 "dve_grammar.yy"
    { (yyval.number) = (yyvsp[-2].number); ;}
    break;

  case 132:
#line 468 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),trans_effect_list_cancel());
	    PERROR(PE_EFFECT_LIST);
	  ;}
    break;

  case 133:
#line 474 "dve_grammar.yy"
    { (yyval.number) = 1; CALL((yylsp[0]),(yylsp[0]),trans_effect_part()); ;}
    break;

  case 134:
#line 476 "dve_grammar.yy"
    { (yyval.number) = (yyvsp[-2].number) + 1; CALL((yylsp[0]),(yylsp[0]),trans_effect_part()); ;}
    break;

  case 135:
#line 491 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),expr_false()); ;}
    break;

  case 136:
#line 493 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),expr_true()); ;}
    break;

  case 137:
#line 495 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),expr_nat((yyvsp[0].number))); ;}
    break;

  case 138:
#line 497 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),expr_id((yyvsp[0].string))); ;}
    break;

  case 139:
#line 499 "dve_grammar.yy"
    { CALL((yylsp[-3]),(yylsp[0]),expr_array_mem((yyvsp[-3].string))); ;}
    break;

  case 140:
#line 501 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_parenthesis()); ;}
    break;

  case 141:
#line 503 "dve_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[0]),expr_unary((yyvsp[-1].number))); ;}
    break;

  case 142:
#line 505 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_LT)); ;}
    break;

  case 143:
#line 507 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_LEQ)); ;}
    break;

  case 144:
#line 509 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_EQ)); ;}
    break;

  case 145:
#line 511 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_NEQ)); ;}
    break;

  case 146:
#line 513 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_GT)); ;}
    break;

  case 147:
#line 515 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_GEQ)); ;}
    break;

  case 148:
#line 517 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_PLUS)); ;}
    break;

  case 149:
#line 519 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_MINUS)); ;}
    break;

  case 150:
#line 521 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_MULT)); ;}
    break;

  case 151:
#line 523 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_DIV)); ;}
    break;

  case 152:
#line 525 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_MOD)); ;}
    break;

  case 153:
#line 527 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_AND)); ;}
    break;

  case 154:
#line 529 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_OR)); ;}
    break;

  case 155:
#line 531 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_XOR)); ;}
    break;

  case 156:
#line 533 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_LSHIFT)); ;}
    break;

  case 157:
#line 535 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_RSHIFT)); ;}
    break;

  case 158:
#line 537 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_BOOL_AND)); ;}
    break;

  case 159:
#line 539 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_BOOL_OR)); ;}
    break;

  case 160:
#line 541 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_state_of_process((yyvsp[-2].string),(yyvsp[0].string))); ;}
    break;

  case 161:
#line 545 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_var_of_process((yyvsp[-2].string),(yyvsp[0].string))); ;}
    break;

  case 162:
#line 547 "dve_grammar.yy"
    { CALL((yylsp[-5]),(yylsp[-3]),expr_var_of_process((yyvsp[-5].string),(yyvsp[-3].string),true)); ;}
    break;

  case 163:
#line 549 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_IMPLY)); ;}
    break;

  case 164:
#line 556 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_assign((yyvsp[-1].number))); ;}
    break;

  case 165:
#line 557 "dve_grammar.yy"
    { PERROR(PE_EXPR); ;}
    break;

  case 166:
#line 562 "dve_grammar.yy"
    { (yyval.number) = T_ASSIGNMENT; ;}
    break;

  case 167:
#line 567 "dve_grammar.yy"
    { (yyval.number) = T_UNARY_MINUS; ;}
    break;

  case 168:
#line 568 "dve_grammar.yy"
    { (yyval.number) = T_TILDE; ;}
    break;

  case 169:
#line 569 "dve_grammar.yy"
    { (yyval.number) = T_BOOL_NOT; ;}
    break;

  case 171:
#line 580 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),system_synchronicity(1,(yyvsp[-1].flag))); ;}
    break;

  case 172:
#line 582 "dve_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),system_synchronicity(0)); ;}
    break;

  case 173:
#line 586 "dve_grammar.yy"
    { (yyval.flag) = false; ;}
    break;

  case 174:
#line 587 "dve_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),system_property((yyvsp[0].string))); (yyval.flag) = true; ;}
    break;


    }

/* Line 1037 of yacc.c.  */
#line 2208 "dve_grammar.tab.cc"

  yyvsp -= yylen;
  yyssp -= yylen;
  yylsp -= yylen;

  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  int yytype = YYTRANSLATE (yychar);
	  const char* yyprefix;
	  char *yymsg;
	  int yyx;

	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  int yyxbegin = yyn < 0 ? -yyn : 0;

	  /* Stay within bounds of both yycheck and yytname.  */
	  int yychecklim = YYLAST - yyn;
	  int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
	  int yycount = 0;

	  yyprefix = ", expecting ";
	  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      {
		yysize += yystrlen (yyprefix) + yystrlen (yytname [yyx]);
		yycount += 1;
		if (yycount == 5)
		  {
		    yysize = 0;
		    break;
		  }
	      }
	  yysize += (sizeof ("syntax error, unexpected ")
		     + yystrlen (yytname[yytype]));
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "syntax error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yyprefix = ", expecting ";
		  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
		      {
			yyp = yystpcpy (yyp, yyprefix);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yyprefix = " or ";
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("syntax error; also virtual memory exhausted");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror ("syntax error");
    }

  yyerror_range[0] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* If at end of input, pop the error token,
	     then the rest of the stack, then return failure.  */
	  if (yychar == YYEOF)
	     for (;;)
	       {
                 yyerror_range[0] = *yylsp;
		 YYPOPSTACK;
		 if (yyssp == yyss)
		   YYABORT;
		 yydestruct ("Error: popping",
                             yystos[*yyssp], yyvsp, yylsp);
	       }
        }
      else
	{
	  yydestruct ("Error: discarding", yytoken, &yylval, &yylloc);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

#ifdef __GNUC__
  /* Pacify GCC when the user code never invokes YYERROR and the label
     yyerrorlab therefore never appears in user code.  */
  if (0)
     goto yyerrorlab;
#endif

  yyerror_range[0] = yylsp[1-yylen];
  yylsp -= yylen;
  yyvsp -= yylen;
  yyssp -= yylen;
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;

      yyerror_range[0] = *yylsp;
      yydestruct ("Error: popping", yystos[yystate], yyvsp, yylsp);
      YYPOPSTACK;
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;

  yyerror_range[1] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the look-ahead.  YYLOC is available though. */
  YYLLOC_DEFAULT (yyloc, yyerror_range - 1, 2);
  *++yylsp = yyloc;

  /* Shift the error token. */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yydestruct ("Error: discarding lookahead",
              yytoken, &yylval, &yylloc);
  yychar = YYEMPTY;
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*----------------------------------------------.
| yyoverflowlab -- parser overflow comes here.  |
`----------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 593 "dve_grammar.yy"



/* BEGIN of definitions of functions provided by parser */

void yyerror(const char * msg)
 { (*pterr) << yylloc.first_line << ":" << yylloc.first_column << "-" <<
		yylloc.last_line << ":" << yylloc.last_column << "  " <<
		msg << psh();
  last_loc = yylloc;
  (*pterr) << "Parsing interrupted." << thr();
 }

void divine::dve_init_parsing(dve_parser_t * const pt, error_vector_t * const estack,
                       istream & mystream)
 { parser = pt; pterr = estack; mylexer.yyrestart(&mystream);
   yylloc.first_line=1; yylloc.first_column=1;
   yylloc.last_line=1; yylloc.last_column=1;
 }

/* END of definitions of functions provided by parser */



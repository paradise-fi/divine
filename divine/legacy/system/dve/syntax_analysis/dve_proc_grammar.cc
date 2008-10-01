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
#define yyparse dve_ppparse
#define yylex   dve_pplex
#define yyerror dve_pperror
#define yylval dve_yylval
#define yychar  dve_ppchar
#define yydebug dve_ppdebug
#define yynerrs dve_ppnerrs
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
#line 5 "dve_proc_grammar.yy"

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
#line 103 "dve_proc_grammar.yy"
typedef union YYSTYPE {
  bool flag;
  int number;
  char string[MAXLEN];
} YYSTYPE;
/* Line 190 of yacc.c.  */
#line 260 "dve_proc_grammar.tab.cc"
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
#line 284 "dve_proc_grammar.tab.cc"

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
#define YYFINAL  6
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   541

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  75
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  70
/* YYNRULES -- Number of rules. */
#define YYNRULES  153
/* YYNRULES -- Number of states. */
#define YYNSTATES  268

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
       0,     0,     3,     5,     6,    11,    12,    17,    18,    20,
      23,    26,    28,    30,    32,    36,    40,    41,    42,    46,
      49,    51,    55,    57,    61,    63,    64,    68,    70,    71,
      78,    84,    85,    88,    91,    95,    97,   101,   103,   107,
     109,   112,   115,   118,   121,   123,   125,   128,   132,   136,
     140,   144,   148,   151,   155,   158,   162,   166,   169,   171,
     175,   178,   182,   185,   188,   192,   194,   197,   199,   203,
     206,   209,   213,   214,   218,   220,   224,   225,   230,   234,
     235,   239,   241,   245,   254,   256,   262,   266,   268,   272,
     280,   282,   283,   284,   289,   290,   295,   296,   300,   301,
     306,   307,   313,   314,   319,   320,   326,   327,   329,   333,
     335,   339,   341,   342,   343,   344,   350,   353,   355,   359,
     361,   363,   365,   367,   372,   376,   379,   383,   387,   391,
     395,   399,   403,   407,   411,   415,   419,   423,   427,   431,
     435,   439,   443,   447,   451,   455,   459,   466,   470,   474,
     478,   480,   482,   484
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short int yyrhs[] =
{
      91,     0,    -1,    29,    -1,    -1,    81,    78,    83,    69,
      -1,    -1,    81,     1,    79,    69,    -1,    -1,     7,    -1,
      80,    82,    -1,     7,     1,    -1,    30,    -1,    31,    -1,
      84,    -1,    83,    70,    84,    -1,    76,    90,    85,    -1,
      -1,    -1,    34,    86,    87,    -1,    34,     1,    -1,   141,
      -1,    71,    88,    72,    -1,    89,    -1,    88,    70,    89,
      -1,   141,    -1,    -1,    62,    32,    63,    -1,    92,    -1,
      -1,    11,    76,    93,    71,    94,    72,    -1,    95,    96,
     100,   114,   118,    -1,    -1,    95,    77,    -1,    95,     1,
      -1,    12,    97,    69,    -1,    98,    -1,    97,    70,    98,
      -1,    76,    -1,    15,    76,    69,    -1,    99,    -1,    99,
     101,    -1,   101,    99,    -1,   112,   102,    -1,   102,   112,
      -1,   102,    -1,   112,    -1,    16,   103,    -1,    16,    24,
     103,    -1,    16,    25,   104,    -1,    16,    28,   104,    -1,
      16,    27,   107,    -1,    16,    26,   107,    -1,    76,    69,
      -1,    76,    70,   103,    -1,   105,    69,    -1,   105,    70,
     104,    -1,    60,   106,    61,    -1,    60,    61,    -1,    76,
      -1,    76,    70,   106,    -1,   108,    69,    -1,   108,    70,
     107,    -1,   109,   110,    -1,    60,    69,    -1,    60,   111,
      69,    -1,    61,    -1,   111,    61,    -1,    76,    -1,    76,
      70,   111,    -1,    14,   113,    -1,    76,    69,    -1,    76,
      70,   113,    -1,    -1,    17,   115,    69,    -1,   116,    -1,
     115,    70,   116,    -1,    -1,    76,    73,   117,   141,    -1,
      76,    73,     1,    -1,    -1,    18,   119,    69,    -1,   120,
      -1,   119,    70,   124,    -1,    76,    65,    76,    71,   125,
     128,   137,    72,    -1,   121,    -1,    76,    33,    71,   123,
      72,    -1,    76,    73,    32,    -1,   122,    -1,   123,    70,
     122,    -1,    65,    76,    71,   125,   128,   137,    72,    -1,
     120,    -1,    -1,    -1,    19,   126,   141,    69,    -1,    -1,
      19,     1,   127,    69,    -1,    -1,    20,   129,    69,    -1,
      -1,    76,     6,   130,   134,    -1,    -1,    76,     6,     1,
     131,    69,    -1,    -1,    76,    74,   132,   134,    -1,    -1,
      76,    74,     1,   133,    69,    -1,    -1,   141,    -1,    71,
     135,    72,    -1,   136,    -1,   135,    70,   136,    -1,   141,
      -1,    -1,    -1,    -1,    23,   138,   140,    69,   139,    -1,
      23,     1,    -1,   142,    -1,   140,    70,   142,    -1,     9,
      -1,     8,    -1,    32,    -1,    76,    -1,    76,    62,   141,
      63,    -1,    60,   141,    61,    -1,   144,   141,    -1,   141,
      46,   141,    -1,   141,    45,   141,    -1,   141,    42,   141,
      -1,   141,    41,   141,    -1,   141,    43,   141,    -1,   141,
      44,   141,    -1,   141,    49,   141,    -1,   141,    50,   141,
      -1,   141,    53,   141,    -1,   141,    52,   141,    -1,   141,
      51,   141,    -1,   141,    39,   141,    -1,   141,    40,   141,
      -1,   141,    38,   141,    -1,   141,    48,   141,    -1,   141,
      47,   141,    -1,   141,    36,   141,    -1,   141,    37,   141,
      -1,    76,    64,    76,    -1,    76,    65,    76,    -1,    76,
      65,    76,    62,   141,    63,    -1,   141,    35,   141,    -1,
     141,   143,   141,    -1,   141,   143,     1,    -1,    34,    -1,
      50,    -1,    59,    -1,    58,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   120,   131,   137,   137,   139,   139,   143,   144,   148,
     149,   153,   154,   159,   160,   164,   168,   169,   169,   170,
     174,   175,   179,   180,   184,   188,   189,   204,   209,   209,
     217,   221,   223,   224,   271,   275,   276,   280,   287,   291,
     292,   293,   297,   298,   299,   300,   304,   305,   306,   307,
     308,   309,   313,   314,   318,   319,   323,   324,   328,   329,
     333,   334,   338,   342,   343,   347,   348,   352,   353,   357,
     361,   362,   369,   371,   375,   376,   380,   380,   381,   387,
     389,   393,   394,   403,   405,   409,   413,   417,   418,   422,
     424,   428,   429,   429,   431,   431,   435,   436,   440,   440,
     442,   442,   443,   443,   445,   445,   449,   450,   451,   455,
     456,   460,   464,   465,   465,   465,   467,   474,   475,   490,
     492,   494,   496,   498,   500,   502,   504,   506,   508,   510,
     512,   514,   516,   518,   520,   522,   524,   526,   528,   530,
     532,   534,   536,   538,   540,   544,   546,   548,   555,   557,
     562,   567,   568,   569
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
  "'}'", "':'", "'?'", "$accept", "Id", "VariableDecl", "@1", "@2",
  "Const", "TypeName", "TypeId", "DeclIdList", "DeclId", "VarInit", "@3",
  "Initializer", "FieldInitList", "FieldInit", "ArrayDecl",
  "ProcDeclStandalone", "ProcDecl", "@4", "ProcBody", "ProcLocalDeclList",
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
  "Expression", "Assignment", "AssignOp", "UnaryOp", 0
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
       0,    75,    76,    78,    77,    79,    77,    80,    80,    81,
      81,    82,    82,    83,    83,    84,    85,    86,    85,    85,
      87,    87,    88,    88,    89,    90,    90,    91,    93,    92,
      94,    95,    95,    95,    96,    97,    97,    98,    99,   100,
     100,   100,   101,   101,   101,   101,   102,   102,   102,   102,
     102,   102,   103,   103,   104,   104,   105,   105,   106,   106,
     107,   107,   108,   109,   109,   110,   110,   111,   111,   112,
     113,   113,   114,   114,   115,   115,   117,   116,   116,   118,
     118,   119,   119,   120,   120,   121,   122,   123,   123,   124,
     124,   125,   126,   125,   127,   125,   128,   128,   130,   129,
     131,   129,   132,   129,   133,   129,   134,   134,   134,   135,
     135,   136,   137,   138,   139,   137,   137,   140,   140,   141,
     141,   141,   141,   141,   141,   141,   141,   141,   141,   141,
     141,   141,   141,   141,   141,   141,   141,   141,   141,   141,
     141,   141,   141,   141,   141,   141,   141,   141,   142,   142,
     143,   144,   144,   144
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     0,     4,     0,     4,     0,     1,     2,
       2,     1,     1,     1,     3,     3,     0,     0,     3,     2,
       1,     3,     1,     3,     1,     0,     3,     1,     0,     6,
       5,     0,     2,     2,     3,     1,     3,     1,     3,     1,
       2,     2,     2,     2,     1,     1,     2,     3,     3,     3,
       3,     3,     2,     3,     2,     3,     3,     2,     1,     3,
       2,     3,     2,     2,     3,     1,     2,     1,     3,     2,
       2,     3,     0,     3,     1,     3,     0,     4,     3,     0,
       3,     1,     3,     8,     1,     5,     3,     1,     3,     7,
       1,     0,     0,     4,     0,     4,     0,     3,     0,     4,
       0,     5,     0,     4,     0,     5,     0,     1,     3,     1,
       3,     1,     0,     0,     0,     5,     2,     1,     3,     1,
       1,     1,     1,     4,     3,     2,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     6,     3,     3,     3,
       1,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       0,     0,     0,    27,     2,    28,     1,     0,    31,     0,
       0,    29,    33,     0,     0,    32,     0,     0,     0,    10,
      37,     0,    35,    11,    12,     9,     5,     0,     0,     0,
       0,    39,    72,     0,    44,    45,    34,     0,     0,    25,
       0,    13,     0,    69,     0,     0,     0,     0,     0,     0,
       0,    46,    40,     0,    79,    41,    43,    42,    36,     6,
       0,    16,     4,     0,    70,     0,    38,    47,     0,    48,
       0,     0,    51,     0,     0,    50,    49,    52,     0,     0,
       0,    74,     0,    30,     0,     0,    15,    14,    71,    57,
      58,     0,    54,     0,    63,    67,     0,    60,     0,    65,
      62,     0,    53,     0,    73,     0,     0,     0,    81,    84,
      26,    19,     0,     0,    56,    55,     0,    64,    61,    66,
      78,     0,    75,     0,     0,    80,     0,   120,   119,   121,
     151,   153,   152,     0,     0,   122,    18,    20,     0,    59,
      68,    77,     0,     0,     0,    90,    82,     0,     0,    22,
      24,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   125,     0,    87,     0,    91,     0,   124,
       0,    21,     0,   144,   145,   147,   142,   143,   139,   137,
     138,   129,   128,   130,   131,   127,   126,   141,   140,   132,
     133,   136,   135,   134,     0,     0,    85,     0,    96,    91,
      23,   123,     0,    86,    88,    94,     0,     0,   112,    96,
       0,     0,     0,     0,     0,     0,     0,   112,   146,    95,
      93,     0,     0,    97,   116,     0,    83,     0,   100,   106,
     104,   106,     0,     0,   117,    89,     0,     0,    99,   107,
       0,   103,   114,     0,   150,     0,   101,     0,   109,   111,
     105,   115,   118,   149,   148,     0,   108,   110
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,   135,    15,    27,    38,    16,    17,    25,    40,    41,
      86,   112,   136,   148,   149,    61,     2,     3,     7,     9,
      10,    18,    21,    22,    31,    32,    33,    34,    51,    69,
      70,    91,    72,    73,    74,   100,    96,    35,    43,    54,
      80,    81,   121,    83,   107,   108,   109,   175,   176,   146,
     208,   216,   221,   218,   224,   239,   246,   241,   250,   248,
     257,   258,   226,   235,   261,   242,   150,   244,   255,   138
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -73
static const short int yypact[] =
{
       0,    14,    68,   -73,   -73,   -73,   -73,   -11,   -73,   -48,
      44,   -73,   -73,    24,    14,   -73,   -14,     9,    43,   -73,
     -73,     2,   -73,   -73,   -73,   -73,   -73,    14,    14,    14,
       6,    49,    59,    98,    96,   101,   -73,    14,    45,    62,
      36,   -73,    51,   -73,    53,    14,    79,    82,    82,    79,
      60,   -73,   -73,    14,   130,   -73,   -73,   -73,   -73,   -73,
     121,   126,   -73,    14,   -73,    14,   -73,   -73,   -22,   -73,
      76,   -21,   -73,    86,   -20,   -73,   -73,   -73,    14,    93,
      95,   -73,    14,   -73,   116,   189,   -73,   -73,   -73,   -73,
     110,   123,   -73,    79,   -73,   122,   117,   -73,    82,   -73,
     -73,   127,   -73,   222,   -73,    14,   -15,   102,   -73,   -73,
     -73,   -73,    99,    14,   -73,   -73,    14,   -73,   -73,   -73,
     -73,   277,   -73,   120,    14,   -73,   -23,   -73,   -73,   -73,
     -73,   -73,   -73,   277,   277,   -50,   -73,   443,   277,   -73,
     -73,   443,    14,   124,    14,   -73,   -73,   396,     8,   -73,
     443,   277,    14,    14,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   -73,   128,   -73,    39,   175,   125,   -73,
     277,   -73,   338,   -73,   137,   461,   477,   477,   316,   316,
     316,   488,   488,    85,    85,    85,    85,   -30,   -30,    50,
      50,   -73,   -73,   -73,   168,    14,   -73,   244,   182,   175,
     -73,   -73,   277,   -73,   -73,   -73,   277,    14,   183,   182,
     367,   136,   303,    -5,   138,   260,   145,   183,   -73,   -73,
     -73,   118,   153,   -73,   -73,   277,   -73,   147,   -73,   206,
     -73,   206,   104,   424,   -73,   -73,   139,   277,   -73,   443,
     140,   -73,   -73,   277,   -73,   266,   -73,    46,   -73,   443,
     -73,   -73,   -73,   -73,   443,   277,   -73,   -73
};

/* YYPGOTO[NTERM-NUM].  */
static const short int yypgoto[] =
{
     -73,    -1,   -73,   -73,   -73,   -73,   -73,   -73,   -73,   157,
     -73,   -73,   -73,   -73,    30,   -73,   -73,   -73,   -73,   -73,
     -73,   -73,   -73,   188,   193,   -73,   196,   194,   -41,   -47,
     -73,   115,   -45,   -73,   -73,   -73,   -69,   198,   169,   -73,
     -73,   131,   -73,   -73,   -73,   107,   -73,    32,   -73,   -73,
      31,   -73,   -73,    22,   -73,   -73,   -73,   -73,   -73,     1,
     -73,   -19,    16,   -73,   -73,   -73,   -72,    -9,   -73,   -73
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -114
static const short int yytable[] =
{
       5,   231,    76,    75,    67,   101,     4,     4,     4,     4,
      26,     1,   151,    20,   152,   153,    23,    24,   123,   168,
     169,   170,   171,   172,    11,    19,    39,    42,    44,    50,
      45,    46,    47,    48,    49,     4,    20,   102,    -3,    89,
     137,    99,   144,     4,    50,    12,   115,   140,    94,   141,
     124,    13,    79,   118,    -8,    -8,    14,    28,    29,    30,
       8,   147,    39,    28,    42,    30,   173,    90,     6,   232,
      95,    36,    37,    95,    -7,    -7,    53,    50,   180,   182,
     181,   106,   185,   186,   187,   188,   189,   190,   191,   192,
     193,   194,   195,   196,   197,   198,   199,   200,   201,   202,
     203,   170,   171,   172,    79,    62,    63,   127,   128,   205,
      28,   206,    90,    29,    59,    95,   265,    30,   266,   238,
      64,    65,    66,   143,    60,   106,   -98,   -98,     4,    77,
      78,   129,   166,   167,   168,   169,   170,   171,   172,    68,
     220,   174,    71,   178,   222,    92,    93,   -98,    82,   130,
     -98,   183,   184,    84,   240,    97,    98,   131,   132,   133,
      85,  -102,  -102,   243,   104,   105,   103,   249,   -98,   249,
     134,   125,   126,   252,   253,   259,   -98,   -98,   -98,   110,
     113,   243,  -102,   264,   114,  -102,   117,   -98,   119,   -98,
     111,   142,   116,   259,   207,   177,   209,   -17,   -17,   212,
     213,   204,   217,  -102,   174,   229,   225,   233,   256,   260,
     210,  -102,  -102,  -102,   127,   128,   223,   236,   -17,   245,
      87,   -17,  -102,   120,  -102,    58,    55,    52,   139,    57,
     -76,   -76,    56,   145,    88,     4,   122,   214,   129,   -17,
     219,   227,   251,   237,   262,   215,   267,   -17,   -17,   -17,
       0,   -76,   -92,   -92,   -76,     0,   130,     0,     0,     0,
     -17,   234,     0,     0,   131,   132,   133,   263,  -113,  -113,
       0,     0,   -76,   -92,   127,   128,   -92,   247,     0,     0,
     -76,   -76,   -76,     0,     0,   127,   128,     0,     0,  -113,
       0,     0,  -113,     0,   -92,     4,     0,     0,   129,     0,
       0,     0,   -92,   -92,   -92,     0,     4,     0,     0,   129,
    -113,     0,     0,     0,     0,     0,   130,     0,  -113,  -113,
    -113,     0,     0,     0,   131,   132,   133,   130,     0,     0,
       0,     0,     0,     0,     0,   131,   132,   133,   154,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   160,   161,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,   172,
       0,     0,   230,   154,   155,   156,   157,   158,   159,   160,
     161,   162,   163,   164,   165,   166,   167,   168,   169,   170,
     171,   172,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   211,   154,   155,   156,   157,   158,   159,   160,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     228,   154,   155,   156,   157,   158,   159,   160,   161,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,   172,
       0,     0,     0,     0,     0,     0,     0,   179,   254,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   154,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   155,   156,   157,
     158,   159,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   157,   158,   159,   160,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,   162,   163,   164,   165,   166,   167,   168,   169,   170,
     171,   172
};

static const short int yycheck[] =
{
       1,     6,    49,    48,    45,    74,    29,    29,    29,    29,
       1,    11,    62,    14,    64,    65,    30,    31,    33,    49,
      50,    51,    52,    53,    72,     1,    27,    28,    29,    30,
      24,    25,    26,    27,    28,    29,    37,    78,    29,    61,
     112,    61,    65,    29,    45,     1,    93,   116,    69,   121,
      65,     7,    53,    98,    30,    31,    12,    14,    15,    16,
      71,   133,    63,    14,    65,    16,   138,    68,     0,    74,
      71,    69,    70,    74,    30,    31,    17,    78,    70,   151,
      72,    82,   154,   155,   156,   157,   158,   159,   160,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,    51,    52,    53,   105,    69,    70,     8,     9,    70,
      14,    72,   113,    15,    69,   116,    70,    16,    72,     1,
      69,    70,    69,   124,    62,   126,     8,     9,    29,    69,
      70,    32,    47,    48,    49,    50,    51,    52,    53,    60,
     212,   142,    60,   144,   216,    69,    70,    29,    18,    50,
      32,   152,   153,    32,     1,    69,    70,    58,    59,    60,
      34,     8,     9,   235,    69,    70,    73,   239,    50,   241,
      71,    69,    70,    69,    70,   247,    58,    59,    60,    63,
      70,   253,    29,   255,    61,    32,    69,    69,    61,    71,
       1,    71,    70,   265,    19,    71,    71,     8,     9,    62,
      32,    73,    20,    50,   205,    69,    23,    69,    69,    69,
     180,    58,    59,    60,     8,     9,   217,    72,    29,    72,
      63,    32,    69,     1,    71,    37,    33,    31,   113,    35,
       8,     9,    34,   126,    65,    29,   105,   205,    32,    50,
     209,   219,   241,   227,   253,     1,   265,    58,    59,    60,
      -1,    29,     8,     9,    32,    -1,    50,    -1,    -1,    -1,
      71,     1,    -1,    -1,    58,    59,    60,     1,     8,     9,
      -1,    -1,    50,    29,     8,     9,    32,    71,    -1,    -1,
      58,    59,    60,    -1,    -1,     8,     9,    -1,    -1,    29,
      -1,    -1,    32,    -1,    50,    29,    -1,    -1,    32,    -1,
      -1,    -1,    58,    59,    60,    -1,    29,    -1,    -1,    32,
      50,    -1,    -1,    -1,    -1,    -1,    50,    -1,    58,    59,
      60,    -1,    -1,    -1,    58,    59,    60,    50,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    58,    59,    60,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      -1,    -1,    69,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    63,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      63,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    61,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,    11,    91,    92,    29,    76,     0,    93,    71,    94,
      95,    72,     1,     7,    12,    77,    80,    81,    96,     1,
      76,    97,    98,    30,    31,    82,     1,    78,    14,    15,
      16,    99,   100,   101,   102,   112,    69,    70,    79,    76,
      83,    84,    76,   113,    76,    24,    25,    26,    27,    28,
      76,   103,   101,    17,   114,    99,   112,   102,    98,    69,
      62,    90,    69,    70,    69,    70,    69,   103,    60,   104,
     105,    60,   107,   108,   109,   107,   104,    69,    70,    76,
     115,   116,    18,   118,    32,    34,    85,    84,   113,    61,
      76,   106,    69,    70,    69,    76,   111,    69,    70,    61,
     110,   111,   103,    73,    69,    70,    76,   119,   120,   121,
      63,     1,    86,    70,    61,   104,    70,    69,   107,    61,
       1,   117,   116,    33,    65,    69,    70,     8,     9,    32,
      50,    58,    59,    60,    71,    76,    87,   141,   144,   106,
     111,   141,    71,    76,    65,   120,   124,   141,    88,    89,
     141,    62,    64,    65,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,   141,    76,   122,   123,    71,    76,    61,
      70,    72,   141,    76,    76,   141,   141,   141,   141,   141,
     141,   141,   141,   141,   141,   141,   141,   141,   141,   141,
     141,   141,   141,   141,    73,    70,    72,    19,   125,    71,
      89,    63,    62,    32,   122,     1,   126,    20,   128,   125,
     141,   127,   141,    76,   129,    23,   137,   128,    63,    69,
      69,     6,    74,    69,     1,   138,    72,   137,     1,   130,
       1,   132,   140,   141,   142,    72,   131,    71,   134,   141,
     133,   134,    69,    70,    34,   143,    69,   135,   136,   141,
      69,   139,   142,     1,   141,    70,    72,   136
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
extern YYSTYPE dve_yylval;

/* Number of syntax errors so far.  */
int yynerrs;
/* Location data for the look-ahead symbol.  */
extern YYLTYPE dve_yylloc;



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
#line 131 "dve_proc_grammar.yy"
    { strncpy((yyval.string), (yyvsp[0].string), MAXLEN); ;}
    break;

  case 3:
#line 137 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),var_decl_begin((yyvsp[0].number))); ;}
    break;

  case 4:
#line 138 "dve_proc_grammar.yy"
    { CALL((yylsp[-3]),(yylsp[0]),var_decl_done()); ;}
    break;

  case 5:
#line 139 "dve_proc_grammar.yy"
    { PERROR(PE_VARDECL); CALL((yylsp[-1]),(yylsp[-1]),var_decl_cancel()); ;}
    break;

  case 7:
#line 143 "dve_proc_grammar.yy"
    { (yyval.number) = false; ;}
    break;

  case 8:
#line 144 "dve_proc_grammar.yy"
    { (yyval.number) = true; ;}
    break;

  case 9:
#line 148 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),type_is_const((yyvsp[-1].number))); (yyval.number) = (yyvsp[0].number); ;}
    break;

  case 10:
#line 149 "dve_proc_grammar.yy"
    { PERROR(PE_VARDECL); ;}
    break;

  case 11:
#line 153 "dve_proc_grammar.yy"
    { (yyval.number) = T_INT; ;}
    break;

  case 12:
#line 154 "dve_proc_grammar.yy"
    { (yyval.number) = T_BYTE; ;}
    break;

  case 15:
#line 164 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]), (yylsp[0]), var_decl_create((yyvsp[-2].string), (yyvsp[-1].number), (yyvsp[0].flag))); ;}
    break;

  case 16:
#line 168 "dve_proc_grammar.yy"
    { (yyval.flag) = false; ;}
    break;

  case 17:
#line 169 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),take_expression()); ;}
    break;

  case 18:
#line 169 "dve_proc_grammar.yy"
    { (yyval.flag) = true; ;}
    break;

  case 19:
#line 170 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),take_expression_cancel()); PERROR(PE_VARINIT); ;}
    break;

  case 20:
#line 174 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),var_init_is_field(false)); ;}
    break;

  case 21:
#line 175 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),var_init_is_field(true)); ;}
    break;

  case 22:
#line 179 "dve_proc_grammar.yy"
    {;}
    break;

  case 23:
#line 180 "dve_proc_grammar.yy"
    {;}
    break;

  case 24:
#line 184 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),var_init_field_part()); ;}
    break;

  case 25:
#line 188 "dve_proc_grammar.yy"
    { (yyval.number) = 0; ;}
    break;

  case 26:
#line 189 "dve_proc_grammar.yy"
    { (yyval.number) = 1; CALL((yylsp[-2]),(yylsp[0]),var_decl_array_size((yyvsp[-1].number))); ;}
    break;

  case 27:
#line 204 "dve_proc_grammar.yy"
    { parser->done(); ;}
    break;

  case 28:
#line 209 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[0]),proc_decl_begin((yyvsp[0].string))); ;}
    break;

  case 29:
#line 210 "dve_proc_grammar.yy"
    { CALL((yylsp[-5]),(yylsp[0]),proc_decl_done()); ;}
    break;

  case 32:
#line 223 "dve_proc_grammar.yy"
    {;}
    break;

  case 33:
#line 224 "dve_proc_grammar.yy"
    { PERROR(PE_LOCALDECL); ;}
    break;

  case 34:
#line 271 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),state_list_done()); ;}
    break;

  case 37:
#line 280 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),state_decl((yyvsp[0].string))); ;}
    break;

  case 38:
#line 287 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),state_init((yyvsp[-1].string))); ;}
    break;

  case 46:
#line 304 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[0]),accept_type(T_BUCHI)); ;}
    break;

  case 47:
#line 305 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),accept_type(T_BUCHI)); ;}
    break;

  case 48:
#line 306 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),accept_type(T_GENBUCHI)); ;}
    break;

  case 49:
#line 307 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),accept_type(T_MULLER)); ;}
    break;

  case 50:
#line 308 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),accept_type(T_RABIN)); ;}
    break;

  case 51:
#line 309 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),accept_type(T_STREETT)); ;}
    break;

  case 52:
#line 313 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),state_accept((yyvsp[-1].string))); ;}
    break;

  case 53:
#line 314 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),state_accept((yyvsp[-2].string))); ;}
    break;

  case 54:
#line 318 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),accept_genbuchi_muller_set_complete()); ;}
    break;

  case 55:
#line 319 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),accept_genbuchi_muller_set_complete()); ;}
    break;

  case 58:
#line 328 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),state_genbuchi_muller_accept((yyvsp[0].string))); ;}
    break;

  case 59:
#line 329 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),state_genbuchi_muller_accept((yyvsp[-2].string))); ;}
    break;

  case 60:
#line 333 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),accept_rabin_streett_pair_complete()); ;}
    break;

  case 61:
#line 334 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),accept_rabin_streett_pair_complete()); ;}
    break;

  case 63:
#line 342 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[0]),accept_rabin_streett_first_complete()); ;}
    break;

  case 64:
#line 343 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),accept_rabin_streett_first_complete()); ;}
    break;

  case 67:
#line 352 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),state_rabin_streett_accept((yyvsp[0].string))); ;}
    break;

  case 68:
#line 353 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),state_rabin_streett_accept((yyvsp[-2].string))); ;}
    break;

  case 70:
#line 361 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),state_commit((yyvsp[-1].string))); ;}
    break;

  case 71:
#line 362 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),state_commit((yyvsp[-2].string))); ;}
    break;

  case 76:
#line 380 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),take_expression()); ;}
    break;

  case 77:
#line 380 "dve_proc_grammar.yy"
    { CALL((yylsp[-3]),(yylsp[-1]),assertion_create((yyvsp[-3].string))); ;}
    break;

  case 78:
#line 381 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),take_expression_cancel()); PERROR(PE_ASSERT); ;}
    break;

  case 83:
#line 404 "dve_proc_grammar.yy"
    { CALL((yylsp[-7]),(yylsp[0]),trans_create((yyvsp[-7].string),(yyvsp[-5].string),(yyvsp[-3].flag),(yyvsp[-2].number),(yyvsp[-1].number))); ;}
    break;

  case 85:
#line 409 "dve_proc_grammar.yy"
    { CALL((yylsp[-4]),(yylsp[0]),prob_trans_create((yyvsp[-4].string))); ;}
    break;

  case 86:
#line 413 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),prob_transition_part((yyvsp[-2].string),(yyvsp[0].number))); ;}
    break;

  case 89:
#line 423 "dve_proc_grammar.yy"
    { CALL((yylsp[-6]),(yylsp[0]),trans_create(0,(yyvsp[-5].string),(yyvsp[-3].flag),(yyvsp[-2].number),(yyvsp[-1].number))); ;}
    break;

  case 91:
#line 428 "dve_proc_grammar.yy"
    { (yyval.flag) = false ;}
    break;

  case 92:
#line 429 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),take_expression()); ;}
    break;

  case 93:
#line 430 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-2]),trans_guard_expr()); (yyval.flag) = true; ;}
    break;

  case 94:
#line 431 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),take_expression_cancel()) ;}
    break;

  case 95:
#line 431 "dve_proc_grammar.yy"
    { (yyval.flag) = false; ;}
    break;

  case 96:
#line 435 "dve_proc_grammar.yy"
    { (yyval.number) = 0; ;}
    break;

  case 97:
#line 436 "dve_proc_grammar.yy"
    { (yyval.number) = (yyvsp[-1].number); ;}
    break;

  case 98:
#line 440 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[0]),take_expression()); ;}
    break;

  case 99:
#line 441 "dve_proc_grammar.yy"
    { CALL((yylsp[-3]),(yylsp[0]),trans_sync((yyvsp[-3].string),1,(yyvsp[0].flag))); (yyval.number) = 1; ;}
    break;

  case 100:
#line 442 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),take_expression_cancel()); ;}
    break;

  case 101:
#line 442 "dve_proc_grammar.yy"
    { (yyval.number) = 1; ;}
    break;

  case 102:
#line 443 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[0]),take_expression()); ;}
    break;

  case 103:
#line 444 "dve_proc_grammar.yy"
    { CALL((yylsp[-3]),(yylsp[0]),trans_sync((yyvsp[-3].string),2,(yyvsp[0].flag))); (yyval.number) = 2; ;}
    break;

  case 104:
#line 445 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[-1]),take_expression_cancel()); ;}
    break;

  case 105:
#line 445 "dve_proc_grammar.yy"
    { (yyval.number) = 1; ;}
    break;

  case 106:
#line 449 "dve_proc_grammar.yy"
    { (yyval.flag) = false; ;}
    break;

  case 107:
#line 450 "dve_proc_grammar.yy"
    { (yyval.flag) = true; ;}
    break;

  case 108:
#line 451 "dve_proc_grammar.yy"
    { (yyval.flag) = true; ;}
    break;

  case 111:
#line 460 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]), expression_list_store()); ;}
    break;

  case 112:
#line 464 "dve_proc_grammar.yy"
    { (yyval.number) = 0; ;}
    break;

  case 113:
#line 465 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),trans_effect_list_begin()); ;}
    break;

  case 114:
#line 465 "dve_proc_grammar.yy"
    { CALL((yylsp[-3]),(yylsp[-3]),trans_effect_list_end()); ;}
    break;

  case 115:
#line 466 "dve_proc_grammar.yy"
    { (yyval.number) = (yyvsp[-2].number); ;}
    break;

  case 116:
#line 468 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[-1]),trans_effect_list_cancel());
	    PERROR(PE_EFFECT_LIST);
	  ;}
    break;

  case 117:
#line 474 "dve_proc_grammar.yy"
    { (yyval.number) = 1; CALL((yylsp[0]),(yylsp[0]),trans_effect_part()); ;}
    break;

  case 118:
#line 476 "dve_proc_grammar.yy"
    { (yyval.number) = (yyvsp[-2].number) + 1; CALL((yylsp[0]),(yylsp[0]),trans_effect_part()); ;}
    break;

  case 119:
#line 491 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),expr_false()); ;}
    break;

  case 120:
#line 493 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),expr_true()); ;}
    break;

  case 121:
#line 495 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),expr_nat((yyvsp[0].number))); ;}
    break;

  case 122:
#line 497 "dve_proc_grammar.yy"
    { CALL((yylsp[0]),(yylsp[0]),expr_id((yyvsp[0].string))); ;}
    break;

  case 123:
#line 499 "dve_proc_grammar.yy"
    { CALL((yylsp[-3]),(yylsp[0]),expr_array_mem((yyvsp[-3].string))); ;}
    break;

  case 124:
#line 501 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_parenthesis()); ;}
    break;

  case 125:
#line 503 "dve_proc_grammar.yy"
    { CALL((yylsp[-1]),(yylsp[0]),expr_unary((yyvsp[-1].number))); ;}
    break;

  case 126:
#line 505 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_LT)); ;}
    break;

  case 127:
#line 507 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_LEQ)); ;}
    break;

  case 128:
#line 509 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_EQ)); ;}
    break;

  case 129:
#line 511 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_NEQ)); ;}
    break;

  case 130:
#line 513 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_GT)); ;}
    break;

  case 131:
#line 515 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_GEQ)); ;}
    break;

  case 132:
#line 517 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_PLUS)); ;}
    break;

  case 133:
#line 519 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_MINUS)); ;}
    break;

  case 134:
#line 521 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_MULT)); ;}
    break;

  case 135:
#line 523 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_DIV)); ;}
    break;

  case 136:
#line 525 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_MOD)); ;}
    break;

  case 137:
#line 527 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_AND)); ;}
    break;

  case 138:
#line 529 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_OR)); ;}
    break;

  case 139:
#line 531 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_XOR)); ;}
    break;

  case 140:
#line 533 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_LSHIFT)); ;}
    break;

  case 141:
#line 535 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_RSHIFT)); ;}
    break;

  case 142:
#line 537 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_BOOL_AND)); ;}
    break;

  case 143:
#line 539 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_BOOL_OR)); ;}
    break;

  case 144:
#line 541 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_state_of_process((yyvsp[-2].string),(yyvsp[0].string))); ;}
    break;

  case 145:
#line 545 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_var_of_process((yyvsp[-2].string),(yyvsp[0].string))); ;}
    break;

  case 146:
#line 547 "dve_proc_grammar.yy"
    { CALL((yylsp[-5]),(yylsp[-3]),expr_var_of_process((yyvsp[-5].string),(yyvsp[-3].string),true)); ;}
    break;

  case 147:
#line 549 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_bin(T_IMPLY)); ;}
    break;

  case 148:
#line 556 "dve_proc_grammar.yy"
    { CALL((yylsp[-2]),(yylsp[0]),expr_assign((yyvsp[-1].number))); ;}
    break;

  case 149:
#line 557 "dve_proc_grammar.yy"
    { PERROR(PE_EXPR); ;}
    break;

  case 150:
#line 562 "dve_proc_grammar.yy"
    { (yyval.number) = T_ASSIGNMENT; ;}
    break;

  case 151:
#line 567 "dve_proc_grammar.yy"
    { (yyval.number) = T_UNARY_MINUS; ;}
    break;

  case 152:
#line 568 "dve_proc_grammar.yy"
    { (yyval.number) = T_TILDE; ;}
    break;

  case 153:
#line 569 "dve_proc_grammar.yy"
    { (yyval.number) = T_BOOL_NOT; ;}
    break;


    }

/* Line 1037 of yacc.c.  */
#line 2138 "dve_proc_grammar.tab.cc"

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


#line 593 "dve_proc_grammar.yy"



/* BEGIN of definitions of functions provided by parser */

void yyerror(const char * msg)
 { (*pterr) << yylloc.first_line << ":" << yylloc.first_column << "-" <<
		yylloc.last_line << ":" << yylloc.last_column << "  " <<
		msg << psh();
  last_loc = yylloc;
  (*pterr) << "Parsing interrupted." << thr();
 }

void divine::dve_proc_init_parsing(dve_parser_t * const pt, error_vector_t * const estack,
                       istream & mystream)
 { parser = pt; pterr = estack; mylexer.yyrestart(&mystream);
   yylloc.first_line=1; yylloc.first_column=1;
   yylloc.last_line=1; yylloc.last_column=1;
 }

/* END of definitions of functions provided by parser */



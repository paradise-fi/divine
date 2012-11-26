/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 1

/* Substitute the variable and function names.  */
#define yyparse utap_parse
#define yylex   utap_lex
#define yyerror utap_error
#define yylval  utap_lval
#define yychar  utap_char
#define yydebug utap_debug
#define yynerrs utap_nerrs
#define yylloc utap_lloc

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     T_ASSIGNMENT = 258,
     T_ASSPLUS = 259,
     T_ASSMINUS = 260,
     T_ASSMULT = 261,
     T_ASSDIV = 262,
     T_ASSMOD = 263,
     T_ASSOR = 264,
     T_ASSAND = 265,
     T_ASSXOR = 266,
     T_ASSLSHIFT = 267,
     T_ASSRSHIFT = 268,
     T_EXCLAM = 269,
     T_INCREMENT = 270,
     T_DECREMENT = 271,
     T_PLUS = 272,
     T_MINUS = 273,
     T_MULT = 274,
     T_DIV = 275,
     T_INF = 276,
     T_SUP = 277,
     T_MOD = 278,
     T_OR = 279,
     T_XOR = 280,
     T_LSHIFT = 281,
     T_RSHIFT = 282,
     T_BOOL_AND = 283,
     T_BOOL_OR = 284,
     T_KW_AND = 285,
     T_KW_OR = 286,
     T_KW_IMPLY = 287,
     T_KW_NOT = 288,
     T_FORALL = 289,
     T_EXISTS = 290,
     T_LT = 291,
     T_LEQ = 292,
     T_EQ = 293,
     T_NEQ = 294,
     T_GEQ = 295,
     T_GT = 296,
     T_FOR = 297,
     T_WHILE = 298,
     T_DO = 299,
     T_BREAK = 300,
     T_CONTINUE = 301,
     T_SWITCH = 302,
     T_IF = 303,
     T_ELSE = 304,
     T_CASE = 305,
     T_DEFAULT = 306,
     T_RETURN = 307,
     T_ASSERT = 308,
     T_PRIORITY = 309,
     T_TYPEDEF = 310,
     T_STRUCT = 311,
     T_CONST = 312,
     T_OLDCONST = 313,
     T_URGENT = 314,
     T_BROADCAST = 315,
     T_TRUE = 316,
     T_FALSE = 317,
     T_META = 318,
     T_SYSTEM = 319,
     T_PROCESS = 320,
     T_STATE = 321,
     T_COMMIT = 322,
     T_INIT = 323,
     T_TRANS = 324,
     T_SELECT = 325,
     T_GUARD = 326,
     T_SYNC = 327,
     T_ASSIGN = 328,
     T_BEFORE = 329,
     T_AFTER = 330,
     T_PROGRESS = 331,
     T_ARROW = 332,
     T_UNCONTROL_ARROW = 333,
     T_DEADLOCK = 334,
     T_EF = 335,
     T_EG = 336,
     T_AF = 337,
     T_AG = 338,
     T_LEADSTO = 339,
     T_RESULTSET = 340,
     T_EF2 = 341,
     T_EG2 = 342,
     T_AF2 = 343,
     T_AG2 = 344,
     T_CONTROL = 345,
     T_MIN_START = 346,
     T_MAX_START = 347,
     T_CONTROL_T = 348,
     T_ERROR = 349,
     T_ID = 350,
     T_TYPENAME = 351,
     T_NAT = 352,
     T_BOOL = 353,
     T_INT = 354,
     T_CHAN = 355,
     T_CLOCK = 356,
     T_VOID = 357,
     T_SCALAR = 358,
     T_NEW = 359,
     T_NEW_DECLARATION = 360,
     T_NEW_LOCAL_DECL = 361,
     T_NEW_INST = 362,
     T_NEW_SYSTEM = 363,
     T_NEW_PARAMETERS = 364,
     T_NEW_INVARIANT = 365,
     T_NEW_GUARD = 366,
     T_NEW_SYNC = 367,
     T_NEW_ASSIGN = 368,
     T_NEW_SELECT = 369,
     T_OLD = 370,
     T_OLD_DECLARATION = 371,
     T_OLD_LOCAL_DECL = 372,
     T_OLD_INST = 373,
     T_OLD_PARAMETERS = 374,
     T_OLD_INVARIANT = 375,
     T_OLD_GUARD = 376,
     T_OLD_ASSIGN = 377,
     T_PROPERTY = 378,
     T_EXPRESSION = 379,
     T_MAX = 380,
     T_MIN = 381,
     UOPERATOR = 382
   };
#endif
/* Tokens.  */
#define T_ASSIGNMENT 258
#define T_ASSPLUS 259
#define T_ASSMINUS 260
#define T_ASSMULT 261
#define T_ASSDIV 262
#define T_ASSMOD 263
#define T_ASSOR 264
#define T_ASSAND 265
#define T_ASSXOR 266
#define T_ASSLSHIFT 267
#define T_ASSRSHIFT 268
#define T_EXCLAM 269
#define T_INCREMENT 270
#define T_DECREMENT 271
#define T_PLUS 272
#define T_MINUS 273
#define T_MULT 274
#define T_DIV 275
#define T_INF 276
#define T_SUP 277
#define T_MOD 278
#define T_OR 279
#define T_XOR 280
#define T_LSHIFT 281
#define T_RSHIFT 282
#define T_BOOL_AND 283
#define T_BOOL_OR 284
#define T_KW_AND 285
#define T_KW_OR 286
#define T_KW_IMPLY 287
#define T_KW_NOT 288
#define T_FORALL 289
#define T_EXISTS 290
#define T_LT 291
#define T_LEQ 292
#define T_EQ 293
#define T_NEQ 294
#define T_GEQ 295
#define T_GT 296
#define T_FOR 297
#define T_WHILE 298
#define T_DO 299
#define T_BREAK 300
#define T_CONTINUE 301
#define T_SWITCH 302
#define T_IF 303
#define T_ELSE 304
#define T_CASE 305
#define T_DEFAULT 306
#define T_RETURN 307
#define T_ASSERT 308
#define T_PRIORITY 309
#define T_TYPEDEF 310
#define T_STRUCT 311
#define T_CONST 312
#define T_OLDCONST 313
#define T_URGENT 314
#define T_BROADCAST 315
#define T_TRUE 316
#define T_FALSE 317
#define T_META 318
#define T_SYSTEM 319
#define T_PROCESS 320
#define T_STATE 321
#define T_COMMIT 322
#define T_INIT 323
#define T_TRANS 324
#define T_SELECT 325
#define T_GUARD 326
#define T_SYNC 327
#define T_ASSIGN 328
#define T_BEFORE 329
#define T_AFTER 330
#define T_PROGRESS 331
#define T_ARROW 332
#define T_UNCONTROL_ARROW 333
#define T_DEADLOCK 334
#define T_EF 335
#define T_EG 336
#define T_AF 337
#define T_AG 338
#define T_LEADSTO 339
#define T_RESULTSET 340
#define T_EF2 341
#define T_EG2 342
#define T_AF2 343
#define T_AG2 344
#define T_CONTROL 345
#define T_MIN_START 346
#define T_MAX_START 347
#define T_CONTROL_T 348
#define T_ERROR 349
#define T_ID 350
#define T_TYPENAME 351
#define T_NAT 352
#define T_BOOL 353
#define T_INT 354
#define T_CHAN 355
#define T_CLOCK 356
#define T_VOID 357
#define T_SCALAR 358
#define T_NEW 359
#define T_NEW_DECLARATION 360
#define T_NEW_LOCAL_DECL 361
#define T_NEW_INST 362
#define T_NEW_SYSTEM 363
#define T_NEW_PARAMETERS 364
#define T_NEW_INVARIANT 365
#define T_NEW_GUARD 366
#define T_NEW_SYNC 367
#define T_NEW_ASSIGN 368
#define T_NEW_SELECT 369
#define T_OLD 370
#define T_OLD_DECLARATION 371
#define T_OLD_LOCAL_DECL 372
#define T_OLD_INST 373
#define T_OLD_PARAMETERS 374
#define T_OLD_INVARIANT 375
#define T_OLD_GUARD 376
#define T_OLD_ASSIGN 377
#define T_PROPERTY 378
#define T_EXPRESSION 379
#define T_MAX 380
#define T_MIN 381
#define UOPERATOR 382




/* Copy the first part of user declarations.  */
#line 40 "parser.yy"


#include "libparser.h"
#include "utap/position.h"

using namespace std;
using namespace UTAP;
using namespace Constants;

#define YYLLOC_DEFAULT(Current, Rhs, N)        		  	        \
    do        								\
      if (N)        							\
        {								\
          (Current).start        = YYRHSLOC (Rhs, 1).start;	        \
          (Current).end          = YYRHSLOC (Rhs, N).end;	        \
        }								\
      else        							\
        {								\
          (Current).start        = (Current).end   =		        \
            YYRHSLOC (Rhs, 0).end;			  	        \
        }								\
    while (0)

#define YYLTYPE position_t

static ParserBuilder *ch;
static syntax_t syntax;
static int syntax_token = 0;

static void utap_error(const char* msg);

static int lexer_flex();
  
static int utap_lex() 
{
  int old;
  if (syntax_token) {
    old = syntax_token;
    syntax_token = 0;
    return old;
  }
  return lexer_flex();
}

static char rootTransId[MAXLEN];

/* Counter used during array parsing. */
static int types = 0;

#define YYERROR_VERBOSE 1

#define CALL(first,last,call) do { ch->setPosition(first.start, last.end); try { ch->call; } catch (TypeException &te) { ch->handleError(te.what()); } } while (0)



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

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 188 "parser.yy"
{
    bool flag;
    int number;
    ParserBuilder::PREFIX prefix;
    kind_t kind;
    char string[MAXLEN];
}
/* Line 193 of yacc.c.  */
#line 421 "parser.tab.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

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


/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 446 "parser.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
	     && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
    YYLTYPE yyls;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE) + sizeof (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
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
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  106
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   6445

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  145
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  152
/* YYNRULES -- Number of rules.  */
#define YYNRULES  409
/* YYNRULES -- Number of states.  */
#define YYNSTATES  832

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   382

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     144,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   130,   139,
     134,   135,     2,     2,   141,     2,   138,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   129,   140,
       2,     2,     2,   128,     2,   125,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,   126,     2,   127,     2,     2,
       2,   136,     2,   137,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   142,     2,   143,     2,     2,     2,     2,
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
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     131,   132,   133
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     6,     9,    12,    15,    18,    21,    24,
      27,    30,    33,    36,    39,    42,    45,    48,    51,    54,
      57,    60,    63,    66,    69,    70,    73,    76,    77,    87,
      88,    91,    95,    98,   103,   108,   110,   114,   118,   120,
     122,   124,   126,   131,   135,   139,   141,   145,   149,   151,
     152,   157,   158,   164,   168,   169,   172,   175,   178,   181,
     184,   187,   190,   193,   196,   201,   206,   207,   216,   219,
     223,   227,   229,   233,   238,   242,   246,   248,   252,   253,
     258,   259,   262,   264,   268,   270,   274,   278,   280,   281,
     284,   285,   290,   291,   297,   302,   307,   311,   313,   317,
     318,   322,   324,   327,   332,   338,   343,   349,   351,   354,
     356,   359,   366,   374,   376,   379,   381,   383,   388,   394,
     396,   398,   400,   402,   404,   406,   408,   410,   412,   415,
     419,   421,   425,   426,   430,   432,   434,   437,   439,   441,
     442,   450,   456,   457,   460,   463,   466,   470,   473,   475,
     479,   481,   486,   491,   495,   498,   499,   503,   507,   509,
     513,   514,   525,   526,   537,   538,   548,   549,   559,   561,
     562,   566,   570,   576,   577,   581,   586,   590,   591,   595,
     599,   602,   606,   609,   613,   614,   618,   622,   623,   626,
     629,   633,   637,   641,   645,   647,   651,   653,   657,   658,
     664,   665,   668,   671,   672,   675,   679,   681,   683,   686,
     687,   698,   699,   708,   714,   715,   722,   728,   729,   738,
     739,   747,   750,   753,   754,   763,   767,   770,   774,   775,
     776,   780,   782,   785,   786,   792,   793,   799,   800,   805,
     806,   812,   814,   818,   820,   822,   824,   826,   827,   833,
     834,   840,   845,   850,   854,   858,   861,   864,   867,   870,
     873,   877,   881,   885,   889,   893,   897,   901,   905,   909,
     913,   917,   921,   925,   929,   933,   937,   941,   945,   951,
     955,   958,   960,   961,   966,   970,   974,   977,   981,   985,
     986,   995,   996,  1005,  1007,  1010,  1013,  1016,  1020,  1024,
    1027,  1031,  1035,  1039,  1043,  1047,  1054,  1061,  1071,  1081,
    1082,  1086,  1090,  1092,  1094,  1096,  1098,  1100,  1102,  1104,
    1106,  1108,  1110,  1112,  1114,  1116,  1118,  1119,  1121,  1125,
    1129,  1130,  1133,  1136,  1138,  1139,  1144,  1148,  1150,  1154,
    1155,  1160,  1161,  1169,  1170,  1179,  1180,  1188,  1189,  1196,
    1197,  1204,  1207,  1211,  1215,  1217,  1219,  1223,  1227,  1228,
    1233,  1234,  1240,  1241,  1246,  1247,  1253,  1259,  1260,  1263,
    1267,  1270,  1272,  1276,  1278,  1283,  1285,  1289,  1293,  1294,
    1298,  1302,  1304,  1308,  1309,  1319,  1320,  1329,  1331,  1332,
    1336,  1341,  1343,  1347,  1350,  1351,  1355,  1357,  1359,  1363,
    1365,  1369,  1379,  1384,  1385,  1388,  1393,  1398,  1399,  1400
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     146,     0,    -1,   104,   147,    -1,   105,   163,    -1,   106,
     199,    -1,   107,   163,    -1,   108,   147,    -1,   109,   169,
      -1,   110,   243,    -1,   114,   213,    -1,   111,   243,    -1,
     112,   216,    -1,   113,   242,    -1,   115,   254,    -1,   116,
     255,    -1,   117,   276,    -1,   118,   148,    -1,   119,   268,
      -1,   120,   280,    -1,   121,   288,    -1,   122,   242,    -1,
     123,   289,    -1,   124,   243,    -1,   163,   152,    -1,    -1,
     148,   149,    -1,     1,   140,    -1,    -1,   189,   151,     3,
     189,   134,   150,   253,   135,   140,    -1,    -1,   134,   135,
      -1,   134,   169,   135,    -1,   158,   161,    -1,   100,    54,
     154,   140,    -1,   100,    54,     1,   140,    -1,   156,    -1,
     154,   141,   156,    -1,   154,   155,   156,    -1,    36,    -1,
     157,    -1,    51,    -1,   189,    -1,   157,   136,   243,   137,
      -1,    64,   159,   140,    -1,    64,     1,   140,    -1,   189,
      -1,   159,   141,   189,    -1,   159,   160,   189,    -1,    36,
      -1,    -1,    76,   142,   162,   143,    -1,    -1,   162,   243,
     129,   243,   140,    -1,   162,   243,   140,    -1,    -1,   163,
     166,    -1,   163,   171,    -1,   163,   183,    -1,   163,   196,
      -1,   163,   164,    -1,   163,   165,    -1,   163,   149,    -1,
     163,   153,    -1,     1,   140,    -1,    74,   142,   242,   143,
      -1,    75,   142,   242,   143,    -1,    -1,   187,   188,   168,
     142,   167,   225,   226,   143,    -1,   134,   135,    -1,   134,
     169,   135,    -1,   134,     1,   135,    -1,   170,    -1,   169,
     141,   170,    -1,   187,   130,   189,   179,    -1,   187,   189,
     179,    -1,   187,   172,   140,    -1,   173,    -1,   172,   141,
     173,    -1,    -1,   188,   174,   179,   175,    -1,    -1,     3,
     176,    -1,   243,    -1,   142,   177,   143,    -1,   178,    -1,
     177,   141,   178,    -1,   188,   129,   176,    -1,   176,    -1,
      -1,   180,   181,    -1,    -1,   136,   243,   137,   181,    -1,
      -1,   136,   187,   137,   182,   181,    -1,   136,     1,   137,
     181,    -1,    55,   187,   184,   140,    -1,    55,     1,   140,
      -1,   185,    -1,   184,   141,   185,    -1,    -1,   188,   186,
     179,    -1,    96,    -1,   195,    96,    -1,    56,   142,   190,
     143,    -1,   195,    56,   142,   190,   143,    -1,    56,   142,
       1,   143,    -1,   195,    56,   142,     1,   143,    -1,    98,
      -1,   195,    98,    -1,    99,    -1,   195,    99,    -1,    99,
     136,   243,   141,   243,   137,    -1,   195,    99,   136,   243,
     141,   243,   137,    -1,   100,    -1,   195,   100,    -1,   101,
      -1,   102,    -1,   103,   136,   243,   137,    -1,   195,   103,
     136,   243,   137,    -1,   189,    -1,    96,    -1,    95,    -1,
     125,    -1,   126,    -1,   127,    -1,    21,    -1,    22,    -1,
     191,    -1,   190,   191,    -1,   187,   192,   140,    -1,   193,
      -1,   192,   141,   193,    -1,    -1,   188,   194,   179,    -1,
      59,    -1,    60,    -1,    59,    60,    -1,    57,    -1,    63,
      -1,    -1,    65,   188,   168,   142,   197,   198,   143,    -1,
     199,   200,   218,   203,   204,    -1,    -1,   199,   166,    -1,
     199,   171,    -1,   199,   183,    -1,    66,   201,   140,    -1,
       1,   140,    -1,   202,    -1,   201,   141,   202,    -1,   189,
      -1,   189,   142,   243,   143,    -1,   189,   142,     1,   143,
      -1,    68,   189,   140,    -1,     1,   140,    -1,    -1,    69,
     205,   140,    -1,    69,     1,   140,    -1,   206,    -1,   205,
     141,   209,    -1,    -1,   189,    77,   189,   142,   207,   212,
     214,   215,   217,   143,    -1,    -1,   189,    78,   189,   142,
     208,   212,   214,   215,   217,   143,    -1,    -1,    77,   189,
     142,   210,   212,   214,   215,   217,   143,    -1,    -1,    78,
     189,   142,   211,   212,   214,   215,   217,   143,    -1,   206,
      -1,    -1,    70,   213,   140,    -1,   188,   129,   187,    -1,
     213,   141,   188,   129,   187,    -1,    -1,    71,   243,   140,
      -1,    71,   243,     1,   140,    -1,    71,     1,   140,    -1,
      -1,    72,   216,   140,    -1,    72,     1,   140,    -1,   243,
      14,    -1,   243,     1,    14,    -1,   243,   128,    -1,   243,
       1,   128,    -1,    -1,    73,   242,   140,    -1,    73,     1,
     140,    -1,    -1,   218,   219,    -1,   218,   220,    -1,    67,
     221,   140,    -1,    67,     1,   140,    -1,    59,   222,   140,
      -1,    59,     1,   140,    -1,   189,    -1,   221,   141,   189,
      -1,   189,    -1,   222,   141,   189,    -1,    -1,   142,   224,
     225,   226,   143,    -1,    -1,   225,   171,    -1,   225,   183,
      -1,    -1,   226,   227,    -1,   226,     1,   140,    -1,   223,
      -1,   140,    -1,   243,   140,    -1,    -1,    42,   134,   242,
     140,   242,   140,   242,   135,   228,   227,    -1,    -1,    42,
     134,   188,   129,   187,   135,   229,   227,    -1,    42,   134,
       1,   135,   227,    -1,    -1,    43,   134,   230,   242,   135,
     227,    -1,    43,   134,     1,   135,   227,    -1,    -1,    44,
     231,   227,    43,   134,   242,   135,   140,    -1,    -1,    48,
     134,   232,   242,   135,   227,   234,    -1,    45,   140,    -1,
      46,   140,    -1,    -1,    47,   134,   242,   135,   233,   142,
     236,   143,    -1,    52,   243,   140,    -1,    52,   140,    -1,
      53,   243,   140,    -1,    -1,    -1,    49,   235,   227,    -1,
     237,    -1,   236,   237,    -1,    -1,    50,   243,   129,   238,
     226,    -1,    -1,    50,     1,   129,   239,   226,    -1,    -1,
      51,   129,   240,   226,    -1,    -1,    51,     1,   129,   241,
     226,    -1,   243,    -1,   242,   141,   243,    -1,    62,    -1,
      61,    -1,    97,    -1,   189,    -1,    -1,   243,   134,   244,
     253,   135,    -1,    -1,   243,   134,   245,     1,   135,    -1,
     243,   136,   243,   137,    -1,   243,   136,     1,   137,    -1,
     134,   243,   135,    -1,   134,     1,   135,    -1,   243,    15,
      -1,    15,   243,    -1,   243,    16,    -1,    16,   243,    -1,
     252,   243,    -1,   243,    36,   243,    -1,   243,    37,   243,
      -1,   243,    38,   243,    -1,   243,    39,   243,    -1,   243,
      41,   243,    -1,   243,    40,   243,    -1,   243,    17,   243,
      -1,   243,    18,   243,    -1,   243,    19,   243,    -1,   243,
      20,   243,    -1,   243,    23,   243,    -1,   243,   130,   243,
      -1,   243,    24,   243,    -1,   243,    25,   243,    -1,   243,
      26,   243,    -1,   243,    27,   243,    -1,   243,    28,   243,
      -1,   243,    29,   243,    -1,   243,   128,   243,   129,   243,
      -1,   243,   138,   189,    -1,   243,   139,    -1,    79,    -1,
      -1,   243,    32,   246,   243,    -1,   243,    30,   243,    -1,
     243,    31,   243,    -1,    33,   243,    -1,   243,   132,   243,
      -1,   243,   131,   243,    -1,    -1,    34,   134,   188,   129,
     187,   135,   247,   243,    -1,    -1,    35,   134,   188,   129,
     187,   135,   248,   243,    -1,   250,    -1,    82,   243,    -1,
      83,   243,    -1,    80,   243,    -1,    83,    19,   243,    -1,
      80,    19,   243,    -1,    81,   243,    -1,    88,   249,   243,
      -1,    89,   249,   243,    -1,    86,   249,   243,    -1,    87,
     249,   243,    -1,   243,    84,   243,    -1,   125,   136,   243,
     126,   243,   137,    -1,   125,   136,   243,   127,   243,   137,
      -1,   125,   142,   243,   143,   136,   243,   126,   243,   137,
      -1,   125,   142,   243,   143,   136,   243,   127,   243,   137,
      -1,    -1,   142,   243,   143,    -1,   243,   251,   243,    -1,
       3,    -1,     4,    -1,     5,    -1,     7,    -1,     8,    -1,
       6,    -1,    10,    -1,     9,    -1,    11,    -1,    12,    -1,
      13,    -1,    18,    -1,    17,    -1,    14,    -1,    -1,   243,
      -1,   253,   141,   243,    -1,   255,   148,   152,    -1,    -1,
     255,   256,    -1,   255,   261,    -1,   171,    -1,    -1,    58,
     257,   258,   140,    -1,    58,     1,   140,    -1,   259,    -1,
     258,   141,   259,    -1,    -1,   189,   260,   179,   176,    -1,
      -1,    65,   188,   267,   142,   262,   275,   143,    -1,    -1,
      65,   188,   267,     1,   142,   263,   275,   143,    -1,    -1,
      65,   188,     1,   142,   264,   275,   143,    -1,    -1,    65,
       1,   142,   265,   275,   143,    -1,    -1,    65,   188,   142,
     266,   275,   143,    -1,   134,   135,    -1,   134,   268,   135,
      -1,   134,     1,   135,    -1,   269,    -1,   272,    -1,   268,
     140,   269,    -1,   268,   140,   272,    -1,    -1,   187,   270,
     189,   179,    -1,    -1,   269,   271,   141,   189,   179,    -1,
      -1,    58,   273,   189,   179,    -1,    -1,   272,   141,   274,
     189,   179,    -1,   276,   277,   218,   203,   281,    -1,    -1,
     276,   256,    -1,    66,   278,   140,    -1,     1,   140,    -1,
     279,    -1,   278,   141,   279,    -1,   189,    -1,   189,   142,
     280,   143,    -1,   243,    -1,   243,     1,   141,    -1,   280,
     141,   243,    -1,    -1,    69,   282,   140,    -1,    69,     1,
     140,    -1,   283,    -1,   282,   141,   285,    -1,    -1,   189,
      77,   189,   142,   284,   287,   215,   217,   143,    -1,    -1,
      77,   189,   142,   286,   287,   215,   217,   143,    -1,   283,
      -1,    -1,    71,   288,   140,    -1,    71,   288,     1,   140,
      -1,   243,    -1,   288,   141,   243,    -1,   290,   294,    -1,
      -1,   290,   294,   144,    -1,   189,    -1,   291,    -1,   292,
     141,   291,    -1,   243,    -1,    90,   129,   243,    -1,    93,
      19,   134,   243,   141,   243,   135,   129,   243,    -1,    80,
      90,   129,   243,    -1,    -1,   293,   295,    -1,    21,   292,
     129,   293,    -1,    22,   292,   129,   293,    -1,    -1,    -1,
      68,   296,   225,    69,   226,    85,   242,   140,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   202,   202,   203,   204,   205,   206,   207,   208,   209,
     210,   211,   212,   213,   214,   215,   216,   217,   218,   219,
     220,   221,   222,   226,   229,   231,   232,   236,   236,   243,
     244,   248,   253,   256,   257,   261,   262,   263,   267,   271,
     272,   276,   277,   281,   282,   286,   287,   288,   292,   294,
     296,   298,   300,   303,   307,   309,   310,   311,   312,   313,
     314,   315,   316,   317,   320,   322,   329,   329,   336,   337,
     338,   342,   343,   347,   350,   356,   362,   363,   367,   367,
     375,   376,   380,   381,   387,   388,   392,   395,   401,   401,
     403,   405,   406,   406,   407,   411,   414,   418,   419,   423,
     423,   431,   434,   437,   440,   443,   446,   449,   452,   455,
     458,   461,   465,   468,   471,   474,   477,   480,   484,   490,
     491,   495,   496,   497,   498,   499,   500,   504,   505,   509,
     516,   517,   521,   521,   529,   530,   531,   532,   533,   541,
     541,   550,   553,   555,   556,   557,   561,   562,   566,   567,
     571,   574,   577,   583,   586,   589,   591,   592,   596,   597,
     601,   601,   607,   607,   616,   616,   621,   621,   626,   629,
     631,   635,   638,   642,   644,   647,   650,   653,   655,   656,
     660,   663,   666,   669,   674,   676,   679,   682,   684,   685,
     689,   690,   694,   695,   699,   702,   708,   711,   721,   721,
     729,   731,   732,   735,   737,   738,   742,   743,   746,   749,
     749,   755,   755,   761,   762,   762,   768,   769,   769,   775,
     775,   779,   782,   785,   785,   791,   794,   797,   803,   806,
     806,   815,   816,   820,   820,   826,   826,   832,   832,   838,
     838,   847,   848,   853,   856,   859,   862,   865,   865,   870,
     870,   875,   878,   881,   882,   885,   888,   891,   894,   897,
     900,   903,   906,   909,   912,   915,   918,   921,   924,   927,
     930,   933,   936,   939,   942,   945,   948,   951,   954,   957,
     960,   963,   966,   966,   971,   974,   977,   980,   983,   986,
     986,   991,   991,   996,   997,  1000,  1003,  1006,  1009,  1012,
    1015,  1018,  1021,  1024,  1027,  1030,  1033,  1036,  1039,  1045,
    1046,  1049,  1055,  1056,  1057,  1058,  1059,  1060,  1061,  1062,
    1063,  1064,  1065,  1070,  1071,  1072,  1077,  1078,  1081,  1091,
    1094,  1096,  1097,  1101,  1102,  1102,  1107,  1110,  1111,  1115,
    1115,  1126,  1126,  1132,  1132,  1138,  1138,  1144,  1144,  1150,
    1150,  1159,  1160,  1161,  1165,  1168,  1169,  1172,  1176,  1176,
    1181,  1181,  1189,  1189,  1194,  1194,  1202,  1205,  1207,  1211,
    1212,  1216,  1217,  1221,  1224,  1230,  1231,  1233,  1238,  1240,
    1241,  1245,  1246,  1250,  1250,  1260,  1260,  1265,  1268,  1270,
    1273,  1279,  1280,  1286,  1288,  1290,  1293,  1298,  1301,  1306,
    1309,  1313,  1317,  1322,  1324,  1325,  1328,  1332,  1334,  1334
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "T_ASSIGNMENT", "T_ASSPLUS",
  "T_ASSMINUS", "T_ASSMULT", "T_ASSDIV", "T_ASSMOD", "T_ASSOR", "T_ASSAND",
  "T_ASSXOR", "T_ASSLSHIFT", "T_ASSRSHIFT", "T_EXCLAM", "T_INCREMENT",
  "T_DECREMENT", "T_PLUS", "T_MINUS", "T_MULT", "T_DIV", "T_INF", "T_SUP",
  "T_MOD", "T_OR", "T_XOR", "T_LSHIFT", "T_RSHIFT", "T_BOOL_AND",
  "T_BOOL_OR", "T_KW_AND", "T_KW_OR", "T_KW_IMPLY", "T_KW_NOT", "T_FORALL",
  "T_EXISTS", "T_LT", "T_LEQ", "T_EQ", "T_NEQ", "T_GEQ", "T_GT", "T_FOR",
  "T_WHILE", "T_DO", "T_BREAK", "T_CONTINUE", "T_SWITCH", "T_IF", "T_ELSE",
  "T_CASE", "T_DEFAULT", "T_RETURN", "T_ASSERT", "T_PRIORITY", "T_TYPEDEF",
  "T_STRUCT", "T_CONST", "T_OLDCONST", "T_URGENT", "T_BROADCAST", "T_TRUE",
  "T_FALSE", "T_META", "T_SYSTEM", "T_PROCESS", "T_STATE", "T_COMMIT",
  "T_INIT", "T_TRANS", "T_SELECT", "T_GUARD", "T_SYNC", "T_ASSIGN",
  "T_BEFORE", "T_AFTER", "T_PROGRESS", "T_ARROW", "T_UNCONTROL_ARROW",
  "T_DEADLOCK", "T_EF", "T_EG", "T_AF", "T_AG", "T_LEADSTO", "T_RESULTSET",
  "T_EF2", "T_EG2", "T_AF2", "T_AG2", "T_CONTROL", "T_MIN_START",
  "T_MAX_START", "T_CONTROL_T", "T_ERROR", "T_ID", "T_TYPENAME", "T_NAT",
  "T_BOOL", "T_INT", "T_CHAN", "T_CLOCK", "T_VOID", "T_SCALAR", "T_NEW",
  "T_NEW_DECLARATION", "T_NEW_LOCAL_DECL", "T_NEW_INST", "T_NEW_SYSTEM",
  "T_NEW_PARAMETERS", "T_NEW_INVARIANT", "T_NEW_GUARD", "T_NEW_SYNC",
  "T_NEW_ASSIGN", "T_NEW_SELECT", "T_OLD", "T_OLD_DECLARATION",
  "T_OLD_LOCAL_DECL", "T_OLD_INST", "T_OLD_PARAMETERS", "T_OLD_INVARIANT",
  "T_OLD_GUARD", "T_OLD_ASSIGN", "T_PROPERTY", "T_EXPRESSION", "'A'",
  "'U'", "'W'", "'?'", "':'", "'&'", "T_MAX", "T_MIN", "UOPERATOR", "'('",
  "')'", "'['", "']'", "'.'", "'''", "';'", "','", "'{'", "'}'", "'\\n'",
  "$accept", "Uppaal", "XTA", "Instantiations", "Instantiation", "@1",
  "OptionalInstanceParameterList", "System", "PriorityDecl", "ChannelList",
  "ChanLessThan", "ChanElement", "ChanExpression", "SysDecl",
  "ProcessList", "ProcLessThan", "Progress", "ProgressMeasureList",
  "Declarations", "BeforeUpdateDecl", "AfterUpdateDecl", "FunctionDecl",
  "@2", "OptionalParameterList", "ParameterList", "Parameter",
  "VariableDecl", "DeclIdList", "DeclId", "@3", "VarInit", "Initializer",
  "FieldInitList", "FieldInit", "ArrayDecl", "@4", "ArrayDecl2", "@5",
  "TypeDecl", "TypeIdList", "TypeId", "@6", "Type", "Id", "NonTypeId",
  "FieldDeclList", "FieldDecl", "FieldDeclIdList", "FieldDeclId", "@7",
  "TypePrefix", "ProcDecl", "@8", "ProcBody", "ProcLocalDeclList",
  "States", "StateDeclList", "StateDecl", "Init", "Transitions",
  "TransitionList", "Transition", "@9", "@10", "TransitionOpt", "@11",
  "@12", "Select", "SelectList", "Guard", "Sync", "SyncExpr", "Assign",
  "LocFlags", "Commit", "Urgent", "CStateList", "UStateList", "Block",
  "@13", "BlockLocalDeclList", "StatementList", "Statement", "@14", "@15",
  "@16", "@17", "@18", "@19", "ElsePart", "@20", "SwitchCaseList",
  "SwitchCase", "@21", "@22", "@23", "@24", "ExprList", "Expression",
  "@25", "@26", "@27", "@28", "@29", "PathExpression", "Assignment",
  "AssignOp", "UnaryOp", "ArgList", "OldXTA", "OldDeclaration",
  "OldVarDecl", "@30", "OldConstDeclIdList", "OldConstDeclId", "@31",
  "OldProcDecl", "@32", "@33", "@34", "@35", "@36", "OldProcParams",
  "OldProcParamList", "OldProcParam", "@37", "@38", "OldProcConstParam",
  "@39", "@40", "OldProcBody", "OldVarDeclList", "OldStates",
  "OldStateDeclList", "OldStateDecl", "OldInvariant", "OldTransitions",
  "OldTransitionList", "OldTransition", "@41", "OldTransitionOpt", "@42",
  "OldGuard", "OldGuardList", "PropertyList", "PropertyList2", "SimpleId",
  "SimpleIdList", "PropertyExpr", "Property", "OptStateQuery", "@43", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,    65,    85,    87,    63,    58,
      38,   380,   381,   382,    40,    41,    91,    93,    46,    39,
      59,    44,   123,   125,    10
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   145,   146,   146,   146,   146,   146,   146,   146,   146,
     146,   146,   146,   146,   146,   146,   146,   146,   146,   146,
     146,   146,   146,   147,   148,   148,   148,   150,   149,   151,
     151,   151,   152,   153,   153,   154,   154,   154,   155,   156,
     156,   157,   157,   158,   158,   159,   159,   159,   160,   161,
     161,   162,   162,   162,   163,   163,   163,   163,   163,   163,
     163,   163,   163,   163,   164,   165,   167,   166,   168,   168,
     168,   169,   169,   170,   170,   171,   172,   172,   174,   173,
     175,   175,   176,   176,   177,   177,   178,   178,   180,   179,
     181,   181,   182,   181,   181,   183,   183,   184,   184,   186,
     185,   187,   187,   187,   187,   187,   187,   187,   187,   187,
     187,   187,   187,   187,   187,   187,   187,   187,   187,   188,
     188,   189,   189,   189,   189,   189,   189,   190,   190,   191,
     192,   192,   194,   193,   195,   195,   195,   195,   195,   197,
     196,   198,   199,   199,   199,   199,   200,   200,   201,   201,
     202,   202,   202,   203,   203,   204,   204,   204,   205,   205,
     207,   206,   208,   206,   210,   209,   211,   209,   209,   212,
     212,   213,   213,   214,   214,   214,   214,   215,   215,   215,
     216,   216,   216,   216,   217,   217,   217,   218,   218,   218,
     219,   219,   220,   220,   221,   221,   222,   222,   224,   223,
     225,   225,   225,   226,   226,   226,   227,   227,   227,   228,
     227,   229,   227,   227,   230,   227,   227,   231,   227,   232,
     227,   227,   227,   233,   227,   227,   227,   227,   234,   235,
     234,   236,   236,   238,   237,   239,   237,   240,   237,   241,
     237,   242,   242,   243,   243,   243,   243,   244,   243,   245,
     243,   243,   243,   243,   243,   243,   243,   243,   243,   243,
     243,   243,   243,   243,   243,   243,   243,   243,   243,   243,
     243,   243,   243,   243,   243,   243,   243,   243,   243,   243,
     243,   243,   246,   243,   243,   243,   243,   243,   243,   247,
     243,   248,   243,   243,   243,   243,   243,   243,   243,   243,
     243,   243,   243,   243,   243,   243,   243,   243,   243,   249,
     249,   250,   251,   251,   251,   251,   251,   251,   251,   251,
     251,   251,   251,   252,   252,   252,   253,   253,   253,   254,
     255,   255,   255,   256,   257,   256,   256,   258,   258,   260,
     259,   262,   261,   263,   261,   264,   261,   265,   261,   266,
     261,   267,   267,   267,   268,   268,   268,   268,   270,   269,
     271,   269,   273,   272,   274,   272,   275,   276,   276,   277,
     277,   278,   278,   279,   279,   280,   280,   280,   281,   281,
     281,   282,   282,   284,   283,   286,   285,   285,   287,   287,
     287,   288,   288,   289,   290,   290,   291,   292,   292,   293,
     293,   293,   293,   294,   294,   294,   294,   295,   296,   295
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     0,     2,     2,     0,     9,     0,
       2,     3,     2,     4,     4,     1,     3,     3,     1,     1,
       1,     1,     4,     3,     3,     1,     3,     3,     1,     0,
       4,     0,     5,     3,     0,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     4,     4,     0,     8,     2,     3,
       3,     1,     3,     4,     3,     3,     1,     3,     0,     4,
       0,     2,     1,     3,     1,     3,     3,     1,     0,     2,
       0,     4,     0,     5,     4,     4,     3,     1,     3,     0,
       3,     1,     2,     4,     5,     4,     5,     1,     2,     1,
       2,     6,     7,     1,     2,     1,     1,     4,     5,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     3,
       1,     3,     0,     3,     1,     1,     2,     1,     1,     0,
       7,     5,     0,     2,     2,     2,     3,     2,     1,     3,
       1,     4,     4,     3,     2,     0,     3,     3,     1,     3,
       0,    10,     0,    10,     0,     9,     0,     9,     1,     0,
       3,     3,     5,     0,     3,     4,     3,     0,     3,     3,
       2,     3,     2,     3,     0,     3,     3,     0,     2,     2,
       3,     3,     3,     3,     1,     3,     1,     3,     0,     5,
       0,     2,     2,     0,     2,     3,     1,     1,     2,     0,
      10,     0,     8,     5,     0,     6,     5,     0,     8,     0,
       7,     2,     2,     0,     8,     3,     2,     3,     0,     0,
       3,     1,     2,     0,     5,     0,     5,     0,     4,     0,
       5,     1,     3,     1,     1,     1,     1,     0,     5,     0,
       5,     4,     4,     3,     3,     2,     2,     2,     2,     2,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     5,     3,
       2,     1,     0,     4,     3,     3,     2,     3,     3,     0,
       8,     0,     8,     1,     2,     2,     2,     3,     3,     2,
       3,     3,     3,     3,     3,     6,     6,     9,     9,     0,
       3,     3,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     0,     1,     3,     3,
       0,     2,     2,     1,     0,     4,     3,     1,     3,     0,
       4,     0,     7,     0,     8,     0,     7,     0,     6,     0,
       6,     2,     3,     3,     1,     1,     3,     3,     0,     4,
       0,     5,     0,     4,     0,     5,     5,     0,     2,     3,
       2,     1,     3,     1,     4,     1,     3,     3,     0,     3,
       3,     1,     3,     0,     9,     0,     8,     1,     0,     3,
       4,     1,     3,     2,     0,     3,     1,     1,     3,     1,
       3,     9,     4,     0,     2,     4,     4,     0,     0,     8
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       0,     0,     0,   142,     0,     0,     0,     0,     0,     0,
       0,     0,   330,   330,   367,     0,     0,     0,     0,     0,
     394,     0,     0,     0,     2,     0,     3,     4,     5,     6,
       0,   137,   134,   135,   138,   101,   107,   109,   113,   115,
     116,     0,     7,    71,     0,     0,   325,     0,     0,   324,
     323,   125,   126,     0,     0,     0,   244,   243,   281,     0,
       0,     0,     0,   309,   309,   309,   309,   121,   245,   122,
     123,   124,     0,   246,     8,   293,     0,    10,    11,     0,
      12,   241,   120,   122,     0,   119,     9,    13,     0,    14,
      15,     0,    16,   362,   358,    17,   354,   355,     0,    18,
     391,    19,    20,    21,   403,    22,     1,    63,     0,     0,
       0,     0,     0,   113,    61,    23,    62,    49,    59,    60,
      55,    56,    57,     0,    29,    58,   143,   144,   145,     0,
     136,     0,     0,     0,     0,    88,     0,   102,   108,   110,
     114,     0,   256,   258,   286,     0,     0,     0,   296,   299,
     294,     0,   295,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   312,   313,   314,   317,   315,   316,   319,   318,
     320,   321,   322,   255,   257,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   282,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     247,     0,     0,   280,     0,   259,     0,   180,   182,     0,
       0,     0,     0,     0,     0,   333,     0,   331,   332,   368,
      26,    25,     0,     0,     0,     0,   364,     0,     0,     0,
     125,   126,     0,     0,     0,   399,   407,   393,     0,     0,
       0,     0,    45,     0,     0,     0,     0,     0,    32,     0,
      76,    78,     0,     0,     0,     0,     0,   127,     0,     0,
      72,    88,    74,    90,     0,     0,     0,     0,     0,   298,
     297,     0,   302,   303,   300,   301,     0,     0,   254,   253,
     266,   267,   268,   269,   270,   272,   273,   274,   275,   276,
     277,   284,   285,     0,   260,   261,   262,   263,   265,   264,
     304,     0,   271,   288,   287,   326,     0,     0,     0,   279,
     311,   181,   183,   242,   171,     0,     0,     0,     0,     0,
     329,    78,    88,    88,   356,   357,     0,     0,   376,   377,
     392,   396,   397,     0,     0,     0,     0,     0,   408,   404,
     395,    96,     0,    97,    99,    44,    48,    43,     0,     0,
       0,     0,     0,     0,     0,    40,     0,    35,    39,    41,
      51,    75,     0,     0,    88,    30,     0,     0,   105,   132,
       0,   130,   103,   128,     0,   117,    73,     0,    89,     0,
       0,     0,     0,     0,     0,   310,     0,     0,     0,   283,
       0,   327,     0,     0,   252,   251,     0,   336,   339,     0,
     337,   347,     0,     0,   349,     0,   363,   359,    88,    88,
       0,     0,     0,     0,   400,     0,   200,    95,     0,    88,
      46,    47,     0,    68,     0,   139,    64,    65,    34,    38,
      33,     0,     0,     0,     0,    77,    66,    80,    31,     0,
      88,   129,     0,     0,     0,     0,     0,   106,   104,     0,
     118,     0,     0,     0,     0,     0,   278,   248,     0,   250,
     172,    88,   335,     0,   367,   345,     0,   351,     0,   367,
       0,   341,   361,   365,   405,   398,   406,   402,     0,     0,
      98,   100,    70,    69,   142,    36,    37,     0,    50,     0,
     200,     0,    79,    27,   133,   131,   111,    90,    92,    90,
       0,   289,   291,   305,   306,     0,   328,     0,   338,     0,
       0,   367,   353,   352,     0,   343,   367,     0,   203,   201,
     202,     0,     0,    42,     0,    53,   203,     0,    81,    82,
     326,    94,    90,    91,   112,     0,     0,     0,     0,   340,
     348,     0,     0,   187,     0,   350,   367,     0,     0,     0,
     140,     0,     0,   187,     0,     0,    87,     0,    84,     0,
     246,     0,    93,   290,   292,     0,     0,   370,   373,     0,
     371,     0,   346,     0,   342,     0,     0,     0,     0,   217,
       0,     0,     0,     0,     0,     0,     0,   207,   198,   206,
     204,     0,   147,   150,     0,   148,     0,    52,    67,     0,
      83,     0,     0,   307,   308,     0,   369,     0,     0,     0,
       0,     0,   378,   188,   189,   344,     0,   205,     0,     0,
       0,   221,   222,     0,   219,   226,     0,     0,     0,   200,
     208,     0,   146,     0,   155,    85,    86,    28,     0,   372,
     154,     0,   196,     0,     0,   194,     0,     0,     0,   366,
     401,     0,     0,     0,     0,     0,     0,     0,     0,   225,
     227,   409,   203,     0,     0,   149,     0,   141,   374,   193,
     192,     0,   191,   190,     0,   153,     0,     0,     0,   381,
       0,     0,     0,     0,     0,     0,   223,     0,     0,   152,
     151,     0,     0,     0,   158,   197,   195,   380,     0,   379,
       0,   213,     0,     0,   216,     0,     0,     0,     0,   199,
     157,     0,     0,   156,     0,     0,     0,   387,   382,   211,
       0,   215,     0,     0,   228,     0,     0,     0,     0,   168,
     159,   383,     0,     0,     0,     0,     0,     0,     0,   231,
     229,   220,   160,   162,     0,     0,   388,   385,   212,   209,
     218,     0,     0,     0,   237,   224,   232,     0,   169,   169,
     164,   166,     0,   177,   388,     0,   235,   233,   239,   203,
     230,     0,   173,   173,   169,   169,     0,     0,   184,   177,
     210,   203,   203,   203,     0,     0,     0,   177,   177,   173,
     173,     0,   389,     0,     0,     0,     0,   184,     0,     0,
       0,   170,     0,     0,   184,   184,   177,   177,   390,   179,
     178,     0,     0,   384,     0,   176,     0,   174,     0,     0,
     184,   184,   186,   185,   386,   175,   161,   163,     0,     0,
     165,   167
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    22,    24,    92,   114,   530,   253,   115,   116,   356,
     432,   357,   358,   117,   241,   349,   248,   434,    25,   118,
     119,   120,   490,   351,    42,    43,   215,   249,   250,   364,
     492,   556,   557,   558,   262,   263,   378,   532,   122,   342,
     343,   419,   216,    84,    73,   256,   257,   370,   371,   440,
      45,   125,   484,   521,    27,   553,   594,   595,   612,   667,
     693,   694,   758,   759,   730,   774,   775,   772,    86,   787,
     778,    78,   796,   571,   613,   614,   646,   643,   589,   629,
     479,   549,   590,   765,   733,   655,   620,   658,   707,   741,
     757,   738,   739,   782,   781,   769,   783,    80,   591,   305,
     306,   293,   535,   536,   154,    75,   204,    76,   392,    87,
      88,   217,   317,   399,   400,   461,   218,   516,   546,   511,
     464,   469,   405,    95,    96,   223,   225,    97,   222,   327,
     509,   510,   543,   569,   570,    99,   649,   678,   679,   746,
     718,   764,   763,   101,   103,   104,   332,   333,   236,   237,
     339,   416
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -751
static const yytype_int16 yypact[] =
{
    2488,  5525,   735,  -751,   735,  5525,  2382,  6225,  6225,  6225,
    6225,   614,  -751,  -751,  -751,   101,  2214,  6225,  6225,  6225,
    -751,  6225,    34,   -82,  -751,  6235,  6318,  2196,  6318,  -751,
     -78,  -751,    18,  -751,  -751,  -751,  -751,   -51,  -751,  -751,
    -751,   -47,   -36,  -751,   470,   -26,  -751,  6225,  6225,  -751,
    -751,  -751,  -751,  6225,   -25,   -14,  -751,  -751,  -751,  6104,
    6225,  6225,  6127,    16,    16,    16,    16,  -751,  -751,   -92,
    -751,  -751,  4724,  -751,  4308,  -751,  6225,  4308,  -751,  2015,
      22,  4308,  -751,  -751,    -2,  -751,    26,  -751,  5538,  2204,
    2374,    55,   504,  -751,  -751,    60,    79,    83,   936,   100,
    4308,   109,    22,  -751,  5885,  4308,  -751,  -751,  1317,   274,
     614,    89,   111,   212,  -751,  -751,  -751,   194,  -751,  -751,
    -751,  -751,  -751,   614,   149,  -751,  -751,  -751,  -751,  1420,
    -751,  6225,  6225,  2382,   504,  -751,   145,  -751,  -751,   158,
    -751,   168,   118,   118,  4523,   614,   614,  6225,  4445,  4445,
    4445,  6225,  4445,  6225,  6225,  6225,  6225,  6225,  6225,  6225,
     150,  3037,  -751,  -751,  -751,  -751,  -751,  -751,  -751,  -751,
    -751,  -751,  -751,  -751,  -751,  6225,  6225,  6225,  6225,  6225,
    6225,  6225,  6225,  6225,  6225,  6225,  6225,  6225,  -751,  6225,
    6225,  6225,  6225,  6225,  6225,  6225,  6225,  6225,  6225,  6225,
     296,  4747,   504,  -751,  6225,   118,     9,  -751,  6225,  6225,
    2382,   614,   287,   120,   117,  -751,   614,  -751,  -751,  -751,
    -751,  -751,   504,   504,  2214,   165,  -751,   174,  6225,  6225,
     504,   504,  5983,   230,   304,  4308,   298,   184,   228,   614,
     241,   -12,  -751,   264,  6225,  6225,   260,   242,  -751,   139,
    -751,   264,  2031,   393,   259,   614,  1804,  -751,  2152,  3076,
    -751,  -751,  -751,   268,  1523,  6225,  6225,   278,   282,   118,
     118,  1753,  4445,  4445,  4445,  4445,  2861,  1792,  -751,  -751,
     538,   538,   118,   118,   118,  5705,  5732,    75,    75,  5678,
    5651,  4523,  4484,  6225,   848,   848,  1708,  1708,   848,   848,
    4445,  3213,  5857,   663,   663,  6225,   418,   285,  3252,  -751,
    4523,  -751,  -751,  4308,  -751,   299,   294,   504,   283,    20,
    -751,  -751,  -751,  -751,    79,    83,   504,   504,  -751,  4308,
    4308,  -751,  -751,   -90,   -70,   310,  6225,   307,  -751,  -751,
    -751,  -751,   161,  -751,  -751,  -751,  -751,  -751,   504,   504,
     275,   301,   -75,   -55,   312,  -751,   124,  -751,   311,  -751,
    -751,  -751,   614,   316,  -751,  -751,   -74,   504,  -751,  -751,
     172,  -751,  -751,  -751,  6225,  -751,  -751,  4610,  -751,   317,
    1812,  2191,  3389,  2382,  2382,  -751,  6225,  6225,   329,  4484,
    6225,  4308,   -54,   333,  -751,  -751,  2382,  -751,  -751,   186,
    -751,  -751,   327,   262,  -751,    17,  -751,  -751,  -751,  -751,
    6006,   504,  6006,  6225,  4308,  6225,  -751,  -751,   614,  -751,
    -751,  -751,   336,  -751,   -27,  -751,  -751,  -751,  -751,  -751,
    -751,   637,   637,  6225,  5208,  -751,  -751,   469,  -751,   341,
    -751,  -751,   614,  3428,   339,   342,  3565,  -751,  -751,  6225,
    -751,   345,   346,  3604,  3741,  6225,   873,  -751,  6225,  -751,
    -751,  -751,  -751,   504,  -751,  -751,   347,  -751,    -4,  -751,
     343,  -751,  -751,  -751,  -751,  -751,  -751,  4308,  2330,  2023,
    -751,  -751,  -751,  -751,  -751,  -751,  -751,  3780,  -751,  2369,
    -751,  5368,  -751,  -751,  -751,  -751,  -751,   268,  -751,   268,
    3917,  -751,  -751,  -751,  -751,  2900,  4308,  5368,  -751,   340,
    1111,  -751,  -751,  -751,   344,  -751,  -751,  6225,  -751,  -751,
    -751,   351,  1214,  -751,  6225,  -751,  2196,  5345,  -751,  4308,
    6225,  -751,   268,  -751,  -751,  6225,  6225,  6225,  6225,  -751,
    -751,   348,   504,  -751,   352,  -751,  -751,   356,  3956,  1613,
    -751,   349,   504,  -751,  2507,  1409,  -751,    62,  -751,   361,
     372,   -22,  -751,  4445,  4445,  4093,  4132,  -751,   363,   196,
    -751,    44,  -751,   360,  -751,   378,   373,   380,   382,  -751,
     375,   379,   387,   388,  5482,  6225,  6225,  -751,  -751,  -751,
    -751,  2546,  -751,   385,   199,  -751,    44,  -751,  -751,  5345,
    -751,  5368,   392,  -751,  -751,  6225,  -751,   504,   395,   371,
     414,   504,   468,  -751,  -751,  -751,  6225,  -751,   686,  4845,
    5231,  -751,  -751,  6225,  -751,  -751,  2684,  2723,   203,  -751,
    -751,  4868,  -751,   504,   473,  -751,  -751,  -751,    67,  -751,
    -751,   398,  -751,   210,   403,  -751,   216,   404,   423,  -751,
    4308,   410,   417,   248,   412,  6225,   509,    58,  6225,  -751,
    -751,  -751,  2196,   416,  1933,  -751,   456,  -751,  -751,  -751,
    -751,   504,  -751,  -751,   504,  -751,   420,   485,   250,  -751,
    5231,  2382,  6225,  5231,   114,   429,  -751,   127,  1511,  -751,
    -751,   424,   270,   254,  -751,  -751,  -751,  -751,   504,  -751,
     701,  -751,   431,   276,  -751,  5231,  6225,   425,  5231,  -751,
    -751,   504,   504,  -751,   569,   426,   504,  -751,  -751,  -751,
    6225,  -751,   136,   381,   521,   430,   433,   504,   504,  -751,
    -751,  -751,   434,  5231,   143,   440,  4966,    14,    -8,  -751,
    -751,  -751,  -751,  -751,   442,   443,   502,  -751,  -751,  -751,
    -751,   457,  4269,   458,  -751,  -751,  -751,  5231,   518,   518,
    -751,  -751,  6225,   517,   502,  5231,  -751,  -751,  -751,  -751,
    -751,   614,   522,   522,   518,   518,    25,  4989,   528,   517,
    -751,  -751,  -751,  -751,  1001,   309,  5087,   517,   517,   522,
     522,   462,  -751,   464,   465,  5110,   463,   528,  1103,  1205,
    1307,  -751,   467,  1974,   528,   528,   517,   517,  -751,  -751,
    -751,   474,   314,  -751,   472,  -751,   477,  -751,   476,   479,
     528,   528,  -751,  -751,  -751,  -751,  -751,  -751,   480,   483,
    -751,  -751
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -751,  -751,   605,   525,   -57,  -751,  -751,   406,  -751,  -751,
    -751,    32,  -751,  -751,  -751,  -751,  -751,  -751,   232,  -751,
    -751,   -11,  -751,   376,  -224,   495,   -20,  -751,   271,  -751,
    -751,  -471,  -751,    33,  -116,  -751,  -472,  -751,   -18,  -751,
     219,  -751,    90,    94,   207,   370,  -225,  -751,   208,  -751,
    -751,  -751,  -751,  -751,   157,  -751,  -751,    12,    56,  -751,
    -751,   -61,  -751,  -751,  -751,  -751,  -751,  -712,  -117,  -741,
    -750,  -122,  -721,   103,  -751,  -751,  -751,  -751,  -751,  -751,
    -468,  -509,  -601,  -751,  -751,  -751,  -751,  -751,  -751,  -751,
    -751,  -751,   -81,  -751,  -751,  -751,  -751,   -15,    -7,  -751,
    -751,  -751,  -751,  -751,   225,  -751,  -751,  -751,   130,  -751,
     648,   -77,  -751,  -751,   200,  -751,  -751,  -751,  -751,  -751,
    -751,  -751,  -751,   263,   438,  -751,  -751,   441,  -751,  -751,
    -436,   654,  -751,  -751,    64,    68,  -751,  -751,   -31,  -751,
    -751,  -751,   -80,   -65,  -751,  -751,   281,   454,  -143,  -751,
    -751,  -751
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -376
static const yytype_int16 yytable[] =
{
      74,    77,    79,    81,   102,   121,   121,   127,   121,   128,
      98,   100,    81,   219,   105,   753,   126,   555,   470,   656,
     528,   402,   526,   311,   346,   531,   791,   533,   366,   797,
     136,   373,   788,   514,   106,   221,   539,   804,   805,   410,
     142,   143,   736,   737,   158,   608,   144,   773,   806,   807,
     159,   411,   148,   149,   150,   152,   820,   821,   107,   412,
     562,   438,   789,   790,   129,   161,   209,   133,   426,   205,
     137,   411,   138,   139,   140,   544,   814,   141,   130,   701,
     547,   457,   704,   818,   819,   131,   209,   458,   427,   132,
     173,   174,   175,   176,   177,   178,    44,   235,   179,   828,
     829,   -24,    91,   609,   721,   133,    94,   724,   483,   145,
     573,   610,   611,   602,   133,   123,   123,   123,   123,   458,
     146,   318,   -24,   -24,   258,   259,   424,   210,   347,   348,
     636,   513,   748,   173,   174,   755,   224,   312,    51,    52,
     269,    51,    52,   754,   270,   376,   271,   272,   273,   274,
     275,   276,   277,   688,   403,   373,   770,   221,   153,   471,
     429,   662,   404,   209,   780,   792,   229,   211,   280,   281,
     282,   283,   284,   285,   286,   287,   288,   289,   290,   291,
     292,   109,   294,   295,   296,   297,   298,   299,   300,   301,
     302,   303,   304,   686,   308,   220,   -24,   310,   239,   209,
     224,   301,   313,   599,   243,   600,   406,   407,   228,   200,
     668,   201,    67,   202,   203,    67,    82,   251,    85,   255,
    -360,   329,   330,    44,   226,   148,   -24,   -24,   -24,   352,
     353,   244,   124,   124,    26,   124,    28,    81,    81,   267,
     268,   228,    83,    70,    71,    83,    70,    71,   437,   705,
     229,   135,   200,   245,   201,   209,   202,   203,   381,   382,
     784,   354,   708,   466,   430,   431,   246,   474,   209,   476,
     247,   735,   798,   799,   800,   240,   422,   209,   749,   361,
     362,    51,    52,   252,   209,   278,   389,   264,   316,   155,
     156,   157,   472,   473,   265,    51,    52,  -249,   391,   124,
     314,   417,   418,   481,   266,   315,   326,   319,  -334,  -334,
     321,   355,   441,   442,    94,   328,   242,    85,    30,    31,
      93,    32,    33,   337,   494,    34,   462,   463,   340,   414,
      85,    30,    31,   344,    32,    33,   606,   607,    34,   632,
     633,   261,    44,   661,   209,   507,   255,   711,   712,   369,
     670,   671,    85,    85,   255,    67,   673,   674,    35,   336,
      36,    37,    38,    39,    40,    41,   338,   443,   341,    67,
     446,    35,   641,    36,    37,    38,    39,    40,    41,   453,
     454,   345,  -334,   456,   360,    83,    70,    71,   682,   209,
     699,   700,    51,    52,   713,   714,   367,   467,   350,    83,
      70,    71,   368,   235,   377,   235,   477,   383,   478,   309,
     423,   384,  -334,  -334,  -334,   644,   720,   209,    85,   393,
      85,   124,   394,    85,   676,   401,   487,   489,   396,   322,
     323,   736,   737,   219,   397,    51,    52,   331,   331,   413,
      44,   415,   500,   425,    51,    52,    85,   433,   505,   801,
     211,   506,   428,   359,   823,   209,   321,   691,   436,   519,
     447,   520,    85,   485,   486,   455,    67,   445,   459,   465,
     255,   482,   491,   451,   452,   493,   497,    51,    52,   498,
     501,   502,   512,   540,   529,   515,   460,   545,   567,   592,
     601,    51,    52,    94,   550,   572,    83,    70,    71,   574,
     529,  -119,   127,   615,   128,   605,   519,   616,   520,    67,
     548,   126,   344,   617,   618,   621,   619,   554,    67,   622,
     529,   623,   624,   391,   398,    51,    52,   631,   563,   564,
     565,   566,   637,   408,   409,   640,   369,   648,   669,    83,
      70,    71,   666,   672,   675,   680,   681,   683,    83,    70,
      71,    67,   685,   173,   174,   420,   421,   177,   178,   689,
     697,   179,   698,   706,   710,    67,   719,   723,   731,    85,
     740,   628,   742,   762,   439,   743,   747,   626,   627,    81,
     750,    83,    70,    71,   760,   761,   766,   768,   771,   777,
      51,    52,   529,   786,   529,    83,    70,    71,    98,    67,
     134,   795,   808,   653,   809,   810,   813,   815,   657,   650,
      29,    81,   123,   214,   822,   824,    81,   825,   331,   826,
     320,   559,   827,   830,   664,    85,   831,   363,   260,    83,
      70,    71,   635,   435,   380,    51,    52,   480,   359,   359,
     684,   522,   519,   687,   520,   665,   727,   728,    81,    85,
     495,    81,   634,   729,   785,   794,   596,   756,    51,    52,
     561,    89,   324,   508,    67,   325,   468,   703,    90,   717,
     398,   639,   200,   638,   201,    81,   202,   203,   173,   174,
     175,   176,   177,   178,   779,   334,   179,   651,   355,   182,
     183,   722,   475,   559,    83,    70,    71,   776,     0,    81,
      46,    47,    48,    49,    50,   734,     0,    51,    52,    67,
      82,     0,   652,    81,     0,     0,     0,     0,     0,    53,
      54,    55,    51,    52,     0,     0,     0,     0,     0,   752,
       0,     0,    67,     0,   560,   -54,    23,     0,     0,    83,
      70,    71,     0,     0,     0,     0,     0,    56,    57,   568,
       0,     0,     0,     0,     0,   100,   -54,   -54,     0,   593,
       0,     0,    83,    70,    71,    58,    59,    60,    61,    62,
      79,   702,    63,    64,    65,    66,     0,     0,   716,   803,
     812,    67,    82,    68,     0,     0,     0,     0,    81,     0,
     -54,   -54,   -54,     0,   -54,   -54,    67,   200,   -54,   201,
     -54,   202,   203,     0,     0,     0,   560,     0,     0,   -54,
     -54,    69,    70,    71,   568,     0,   642,   645,   647,     0,
      72,     0,     0,     0,     0,   560,    83,    70,    71,     0,
     -54,   -54,     0,   -54,   -54,   -54,   -54,   -54,   -54,     0,
     593,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   677,     0,     0,     0,     0,
     -54,   -54,   -54,   173,   174,   175,   176,   177,   178,     0,
       0,   179,     0,   692,   182,   183,     0,     0,   695,     0,
       0,   696,     0,     0,     0,     0,     0,     0,   173,   174,
     175,   176,   177,   178,     0,     0,   179,   180,   181,   182,
     183,   184,   185,     0,     0,   715,     0,   677,     0,   189,
     190,   191,   192,   193,   194,     0,     0,     0,   725,   726,
       0,   692,     0,   732,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   744,   745,  -375,   227,     0,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,   172,
       0,   173,   174,   175,   176,   177,   178,     0,     0,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,     0,
       0,     0,   189,   190,   191,   192,   193,   194,    85,   198,
     199,     0,   200,     0,   201,     0,   202,   203,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   196,   576,   197,   198,   199,     0,   200,     0,   201,
       0,   202,   203,     0,     0,    46,    47,    48,    49,    50,
     195,     0,    51,    52,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    53,    54,    55,     0,     0,     0,
       0,     0,     0,   577,   578,   579,   580,   581,   582,   583,
       0,  -238,  -238,   584,   585,     0,     0,     0,     0,     0,
       0,     0,    56,    57,   196,     0,   197,   198,   199,     0,
     200,     0,   201,     0,   202,   203,     0,  -375,     0,  -375,
      58,    59,    60,    61,    62,     0,     0,    63,    64,    65,
      66,     0,     0,     0,     0,     0,    67,     0,    68,     0,
       0,     0,     0,     0,   576,     0,     0,     0,     0,     0,
       0,     0,   541,     0,     0,     0,     0,    46,    47,    48,
      49,    50,     0,     0,    51,    52,    69,    70,    71,     0,
       0,     0,     0,     0,     0,    72,    53,    54,    55,     0,
       0,   587,     0,   588,  -238,   577,   578,   579,   580,   581,
     582,   583,     0,  -236,  -236,   584,   585,     0,     0,     0,
       0,     0,     0,     0,    56,    57,     0,    30,    31,   212,
      32,    33,     0,     0,    34,     0,     0,   542,     0,     0,
       0,     0,    58,    59,    60,    61,    62,     0,     0,    63,
      64,    65,    66,     0,     0,     0,     0,     0,    67,     0,
      68,     0,     0,     0,     0,     0,   576,    35,     0,    36,
      37,    38,    39,    40,    41,   551,     0,     0,     0,    46,
      47,    48,    49,    50,     0,     0,    51,    52,    69,    70,
      71,     0,     0,     0,     0,     0,     0,    72,    53,    54,
      55,     0,     0,   587,     0,   588,  -236,   577,   578,   579,
     580,   581,   582,   583,     0,  -234,  -234,   584,   585,     0,
       0,     0,     0,     0,     0,     0,    56,    57,     0,   108,
      30,    31,     0,    32,    33,     0,     0,    34,     0,     0,
     552,     0,     0,     0,    58,    59,    60,    61,    62,     0,
       0,    63,    64,    65,    66,     0,     0,     0,     0,     0,
      67,     0,    68,     0,     0,     0,     0,     0,   576,     0,
      35,     0,    36,    37,    38,    39,    40,    41,   238,     0,
       0,    46,    47,    48,    49,    50,     0,     0,    51,    52,
      69,    70,    71,     0,     0,     0,     0,     0,     0,    72,
      53,    54,    55,     0,     0,   587,     0,   588,  -234,   577,
     578,   579,   580,   581,   582,   583,     0,  -240,  -240,   584,
     585,     0,     0,     0,     0,     0,     0,     0,    56,    57,
       0,     0,     0,    30,    31,     0,    32,    33,     0,     0,
      34,     0,     0,     0,     0,     0,    58,    59,    60,    61,
      62,     0,     0,    63,    64,    65,    66,     0,     0,     0,
       0,     0,    67,     0,    68,     0,     0,     0,     0,     0,
     576,     0,     0,    35,     0,    36,    37,    38,    39,    40,
      41,   254,     0,    46,    47,    48,    49,    50,     0,     0,
      51,    52,    69,    70,    71,     0,     0,     0,     0,     0,
       0,    72,    53,    54,    55,     0,     0,   587,     0,   588,
    -240,   577,   578,   579,   580,   581,   582,   583,     0,     0,
       0,   584,   585,     0,     0,     0,     0,     0,     0,     0,
      56,    57,     0,     0,     0,     0,    30,    31,     0,    32,
      33,     0,     0,    34,     0,     0,     0,     0,    58,    59,
      60,    61,    62,     0,     0,    63,    64,    65,    66,     0,
       0,     0,     0,     0,    67,     0,    68,     0,     0,     0,
       0,     0,   576,     0,     0,     0,    35,     0,    36,    37,
      38,    39,    40,    41,   379,    46,    47,    48,    49,    50,
       0,     0,    51,    52,    69,    70,    71,     0,     0,     0,
       0,     0,     0,    72,    53,    54,    55,     0,     0,   587,
       0,   588,   598,   577,   578,   579,   580,   581,   582,   583,
       0,     0,     0,   584,   585,     0,     0,     0,     0,     0,
       0,     0,    56,    57,     0,     0,     0,     0,     0,    30,
      31,     0,    32,    33,     0,     0,    34,     0,     0,     0,
      58,    59,    60,    61,    62,     0,     0,    63,    64,    65,
      66,     0,     0,     0,     0,     0,    67,     0,    68,     0,
       0,     0,     0,     0,   576,     0,     0,     0,     0,    35,
       0,    36,    37,    38,    39,    40,    41,    46,    47,    48,
      49,    50,     0,     0,    51,    52,    69,    70,    71,     0,
       0,     0,     0,     0,     0,    72,    53,    54,    55,     0,
       0,   587,     0,   588,   709,   577,   578,   579,   580,   581,
     582,   583,     0,     0,     0,   584,   585,     0,     0,     0,
       0,     0,     0,     0,    56,    57,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    58,    59,    60,    61,    62,     0,   586,    63,
      64,    65,    66,     0,     0,     0,     0,     0,    67,     0,
      68,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   173,   174,   175,   176,   177,   178,     0,
       0,   179,     0,     0,   182,   183,     0,     0,    69,    70,
      71,     0,     0,     0,   189,   190,     0,    72,   193,   194,
       0,     0,     0,   587,     0,   588,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,     0,   173,   174,
     175,   176,   177,   178,     0,     0,   179,   180,   181,   182,
     183,   184,   185,   186,   187,   188,     0,     0,     0,   189,
     190,   191,   192,   193,   194,   162,   163,   164,   165,   166,
     167,   168,   169,   170,   171,   172,     0,   173,   174,   175,
     176,   177,   178,     0,     0,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,     0,     0,     0,   189,   190,
     191,   192,   193,   194,     0,     0,     0,   195,     0,   198,
     199,     0,   200,     0,   201,     0,   202,   203,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      30,    31,     0,    32,    33,     0,     0,    34,    30,    31,
       0,    32,    33,     0,     0,    34,   195,     0,     0,     0,
       0,   196,     0,   197,   198,   199,     0,   200,     0,   201,
       0,   202,   203,     0,     0,     0,   385,     0,     0,     0,
      35,     0,    36,    37,    38,    39,    40,    41,    35,     0,
      36,    37,    38,    39,    40,    41,     0,     0,     0,     0,
     196,     0,   197,   198,   199,     0,   200,     0,   201,     0,
     202,   203,     0,     0,     0,   388,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   372,   173,   174,
     175,   176,   177,   178,     0,   448,   179,   180,   181,   182,
     183,   184,   185,   186,   187,   188,     0,     0,     0,   189,
     190,   191,   192,   193,   194,   816,     0,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,     0,   173,
     174,   175,   176,   177,   178,     0,     0,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,     0,     0,     0,
     189,   190,   191,   192,   193,   194,   206,   195,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   207,
     173,   174,   175,   176,   177,   178,     0,     0,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   188,     0,     0,
       0,   189,   190,   191,   192,   193,   194,     0,   195,     0,
       0,   196,     0,   197,   198,   199,     0,   200,     0,   201,
       0,   202,   203,     0,     0,     0,   690,     0,   108,    30,
      31,     0,    32,    33,     0,     0,    34,    30,    31,     0,
      32,    33,   518,     0,    34,     0,     0,     0,     0,   195,
       0,     0,   196,     0,   197,   198,   199,     0,   200,     0,
     201,     0,   202,   203,   817,     0,     0,     0,     0,    35,
       0,    36,    37,    38,    39,    40,    41,    35,     0,    36,
      37,    38,    39,    40,    41,     0,     0,     0,     0,     0,
       0,     0,     0,   208,     0,   197,   198,   199,     0,   200,
       0,   201,     0,   202,   203,   162,   163,   164,   165,   166,
     167,   168,   169,   170,   171,   172,   365,   173,   174,   175,
     176,   177,   178,     0,     0,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,     0,     0,     0,   189,   190,
     191,   192,   193,   194,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,     0,   173,   174,   175,   176,
     177,   178,     0,     0,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,     0,     0,     0,   189,   190,   191,
     192,   193,   194,     0,     0,     0,   195,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   108,    30,    31,     0,    32,    33,     0,     0,    34,
      30,    31,   212,    32,    33,     0,     0,    34,     0,   213,
      30,    31,    93,    32,    33,   195,     0,    34,     0,     0,
     196,     0,   197,   198,   199,     0,   200,     0,   201,     0,
     202,   203,    35,   374,    36,    37,    38,    39,    40,    41,
      35,     0,    36,    37,    38,    39,    40,    41,     0,     0,
      35,     0,    36,    37,    38,    39,    40,    41,     0,   196,
       0,   197,   198,   199,     0,   200,     0,   201,     0,   202,
     203,     0,   449,   162,   163,   164,   165,   166,   167,   168,
     169,   170,   171,   172,     0,   173,   174,   175,   176,   177,
     178,     0,     0,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,     0,     0,     0,   189,   190,   191,   192,
     193,   194,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,     0,   173,   174,   175,   176,   177,   178,
       0,     0,   179,   180,   181,   182,   183,   184,   185,   186,
     187,   188,     0,     0,     0,   189,   190,   191,   192,   193,
     194,     0,     0,     0,   195,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      30,    31,   212,    32,    33,     0,     0,    34,    30,    31,
       0,    32,    33,     0,     0,    34,     0,     0,     0,     0,
       0,     0,     0,   195,     0,     0,     0,     0,   196,     0,
     197,   198,   199,     0,   200,     0,   201,     0,   202,   203,
      35,   517,    36,    37,    38,    39,    40,    41,    35,     0,
      36,    37,    38,    39,    40,    41,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   196,   524,   197,
     198,   199,     0,   200,     0,   201,     0,   202,   203,   525,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,     0,   173,   174,   175,   176,   177,   178,     0,     0,
     179,   180,   181,   182,   183,   184,   185,   186,   187,   188,
       0,     0,     0,   189,   190,   191,   192,   193,   194,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,   172,
       0,   173,   174,   175,   176,   177,   178,     0,     0,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,     0,
       0,     0,   189,   190,   191,   192,   193,   194,     0,     0,
       0,   195,     1,     2,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     195,     0,     0,     0,     0,   196,     0,   197,   198,   199,
       0,   200,     0,   201,     0,   202,   203,   597,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   196,     0,   197,   198,   199,     0,
     200,     0,   201,     0,   202,   203,   630,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,     0,   173,
     174,   175,   176,   177,   178,     0,     0,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,     0,     0,     0,
     189,   190,   191,   192,   193,   194,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,     0,   173,   174,
     175,   176,   177,   178,     0,     0,   179,   180,   181,   182,
     183,   184,   185,   186,   187,   188,     0,     0,     0,   189,
     190,   191,   192,   193,   194,     0,     0,     0,   195,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   195,     0,     0,
       0,     0,   196,     0,   197,   198,   199,     0,   200,     0,
     201,     0,   202,   203,   659,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   196,     0,   197,   198,   199,     0,   200,     0,   201,
       0,   202,   203,   660,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,     0,   173,   174,   175,   176,
     177,   178,     0,     0,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,     0,     0,     0,   189,   190,   191,
     192,   193,   194,   162,   163,   164,   165,   166,   167,   168,
     169,   170,   171,   172,     0,   173,   174,   175,   176,   177,
     178,     0,     0,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,     0,     0,     0,   189,   190,   191,   192,
     193,   194,     0,     0,     0,   195,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   195,     0,     0,   386,   387,   196,
       0,   197,   198,   199,     0,   200,     0,   201,     0,   202,
     203,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   537,   538,   196,     0,
     197,   198,   199,     0,   200,     0,   201,     0,   202,   203,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,     0,   173,   174,   175,   176,   177,   178,     0,     0,
     179,   180,   181,   182,   183,   184,   185,   186,   187,   188,
       0,     0,     0,   189,   190,   191,   192,   193,   194,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,   172,
       0,   173,   174,   175,   176,   177,   178,     0,     0,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,     0,
       0,     0,   189,   190,   191,   192,   193,   194,     0,     0,
       0,   195,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     195,     0,     0,     0,     0,   196,     0,   197,   198,   199,
       0,   200,   279,   201,     0,   202,   203,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   196,     0,   197,   198,   199,     0,
     200,     0,   201,   375,   202,   203,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,     0,   173,   174,
     175,   176,   177,   178,     0,     0,   179,   180,   181,   182,
     183,   184,   185,   186,   187,   188,     0,     0,     0,   189,
     190,   191,   192,   193,   194,   162,   163,   164,   165,   166,
     167,   168,   169,   170,   171,   172,     0,   173,   174,   175,
     176,   177,   178,     0,     0,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,     0,     0,     0,   189,   190,
     191,   192,   193,   194,     0,     0,     0,   195,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   195,     0,     0,     0,
       0,   196,   390,   197,   198,   199,     0,   200,     0,   201,
       0,   202,   203,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     196,     0,   197,   198,   199,     0,   200,     0,   201,   395,
     202,   203,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,     0,   173,   174,   175,   176,   177,   178,
       0,     0,   179,   180,   181,   182,   183,   184,   185,   186,
     187,   188,     0,     0,     0,   189,   190,   191,   192,   193,
     194,   162,   163,   164,   165,   166,   167,   168,   169,   170,
     171,   172,     0,   173,   174,   175,   176,   177,   178,     0,
       0,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,     0,     0,     0,   189,   190,   191,   192,   193,   194,
       0,     0,     0,   195,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   195,     0,     0,     0,     0,   196,     0,   197,
     198,   199,     0,   200,     0,   201,   450,   202,   203,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   196,     0,   197,   198,
     199,     0,   200,     0,   201,   496,   202,   203,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,     0,
     173,   174,   175,   176,   177,   178,     0,     0,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   188,     0,     0,
       0,   189,   190,   191,   192,   193,   194,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,     0,   173,
     174,   175,   176,   177,   178,     0,     0,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,     0,     0,     0,
     189,   190,   191,   192,   193,   194,     0,     0,     0,   195,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   195,     0,
       0,     0,     0,   196,     0,   197,   198,   199,     0,   200,
       0,   201,   499,   202,   203,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   196,     0,   197,   198,   199,     0,   200,     0,
     201,   503,   202,   203,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,     0,   173,   174,   175,   176,
     177,   178,     0,     0,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,     0,     0,     0,   189,   190,   191,
     192,   193,   194,   162,   163,   164,   165,   166,   167,   168,
     169,   170,   171,   172,     0,   173,   174,   175,   176,   177,
     178,     0,     0,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,     0,     0,     0,   189,   190,   191,   192,
     193,   194,     0,     0,     0,   195,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   195,     0,     0,     0,     0,   196,
       0,   197,   198,   199,     0,   200,     0,   201,   504,   202,
     203,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   196,     0,
     197,   198,   199,     0,   200,     0,   201,   523,   202,   203,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,     0,   173,   174,   175,   176,   177,   178,     0,     0,
     179,   180,   181,   182,   183,   184,   185,   186,   187,   188,
       0,     0,     0,   189,   190,   191,   192,   193,   194,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,   172,
       0,   173,   174,   175,   176,   177,   178,     0,     0,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,     0,
       0,     0,   189,   190,   191,   192,   193,   194,     0,     0,
       0,   195,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     195,     0,     0,     0,     0,   196,     0,   197,   198,   199,
       0,   200,     0,   201,   534,   202,   203,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   196,     0,   197,   198,   199,     0,
     200,   575,   201,     0,   202,   203,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,     0,   173,   174,
     175,   176,   177,   178,     0,     0,   179,   180,   181,   182,
     183,   184,   185,   186,   187,   188,     0,     0,     0,   189,
     190,   191,   192,   193,   194,   162,   163,   164,   165,   166,
     167,   168,   169,   170,   171,   172,     0,   173,   174,   175,
     176,   177,   178,     0,     0,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,     0,     0,     0,   189,   190,
     191,   192,   193,   194,     0,     0,     0,   195,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   195,     0,     0,     0,
       0,   196,     0,   197,   198,   199,     0,   200,     0,   201,
     603,   202,   203,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     196,     0,   197,   198,   199,     0,   200,     0,   201,   604,
     202,   203,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,     0,   173,   174,   175,   176,   177,   178,
       0,     0,   179,   180,   181,   182,   183,   184,   185,   186,
     187,   188,     0,     0,     0,   189,   190,   191,   192,   193,
     194,   162,   163,   164,   165,   166,   167,   168,   169,   170,
     171,   172,     0,   173,   174,   175,   176,   177,   178,     0,
       0,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,     0,     0,     0,   189,   190,   191,   192,   193,   194,
       0,     0,     0,   195,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   195,     0,     0,     0,     0,   196,   767,   197,
     198,   199,     0,   200,     0,   201,     0,   202,   203,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   196,     0,   197,   198,
     199,     0,   200,     0,   201,     0,   202,   203,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,     0,
     173,   174,   175,   176,   177,   178,     0,     0,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   188,     0,     0,
       0,   189,   190,   191,   192,   193,   194,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,     0,   173,
     174,   175,   176,   177,   178,     0,     0,   179,   180,   181,
     182,   183,   184,   185,   186,     0,     0,     0,     0,     0,
     189,   190,   191,   192,   193,   194,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,     0,   173,   174,
     175,   176,   177,   178,     0,     0,   179,   180,   181,   182,
     183,   184,   185,     0,     0,     0,     0,     0,     0,   189,
     190,   191,   192,   193,   194,     0,     0,     0,     0,     0,
       0,     0,     0,   196,     0,   197,   198,   199,     0,   200,
       0,   201,     0,   202,   203,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   444,   196,     0,   197,   198,   199,     0,   200,     0,
     201,     0,   202,   203,    46,    47,    48,    49,    50,     0,
       0,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    53,    54,    55,     0,     0,     0,     0,
       0,   196,     0,   197,   198,   199,     0,   200,     0,   201,
       0,   202,   203,     0,     0,     0,    30,    31,     0,    32,
      33,    56,    57,    34,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    58,
      59,    60,    61,    62,     0,     0,    63,    64,    65,    66,
       0,     0,     0,     0,     0,    67,    35,    68,    36,    37,
      38,    39,    40,    41,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   160,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    69,    70,    71,    46,    47,
      48,    49,    50,     0,    72,    51,    52,     0,   307,     0,
       0,     0,     0,     0,     0,     0,     0,    53,    54,    55,
       0,    46,    47,    48,    49,    50,     0,     0,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,    54,    55,     0,     0,    56,    57,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    58,    59,    60,    61,    62,    56,    57,
      63,    64,    65,    66,     0,     0,     0,     0,     0,    67,
       0,    68,     0,     0,     0,     0,    58,    59,    60,    61,
      62,     0,     0,    63,    64,    65,    66,     0,     0,     0,
       0,     0,    67,     0,    68,     0,   654,     0,     0,    69,
      70,    71,     0,     0,     0,     0,     0,     0,    72,  -214,
    -214,  -214,  -214,  -214,     0,     0,  -214,  -214,     0,   663,
       0,     0,    69,    70,    71,     0,     0,     0,  -214,  -214,
    -214,    72,    46,    47,    48,    49,    50,     0,     0,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,    54,    55,     0,     0,  -214,  -214,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -214,  -214,  -214,  -214,  -214,    56,
      57,  -214,  -214,  -214,  -214,     0,     0,     0,     0,     0,
    -214,     0,  -214,     0,     0,     0,     0,    58,    59,    60,
      61,    62,     0,     0,    63,    64,    65,    66,     0,     0,
       0,     0,     0,    67,     0,    68,     0,   751,     0,     0,
    -214,  -214,  -214,     0,     0,     0,     0,     0,     0,  -214,
      46,    47,    48,    49,    50,     0,     0,    51,    52,     0,
     793,     0,     0,    69,    70,    71,     0,     0,     0,    53,
      54,    55,    72,    46,    47,    48,    49,    50,     0,     0,
      51,    52,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    53,    54,    55,     0,     0,    56,    57,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    58,    59,    60,    61,    62,
      56,    57,    63,    64,    65,    66,     0,     0,     0,     0,
       0,    67,     0,    68,     0,     0,     0,     0,    58,    59,
      60,    61,    62,     0,     0,    63,    64,    65,    66,     0,
       0,     0,     0,     0,    67,     0,    68,     0,   802,     0,
       0,    69,    70,    71,     0,     0,     0,     0,     0,     0,
      72,    46,    47,    48,    49,    50,     0,     0,    51,    52,
       0,   811,     0,     0,    69,    70,    71,     0,     0,     0,
      53,    54,    55,    72,    46,    47,    48,    49,    50,     0,
       0,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    53,    54,    55,     0,     0,    56,    57,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    58,    59,    60,    61,
      62,    56,    57,    63,    64,    65,    66,     0,     0,     0,
       0,     0,    67,     0,    68,     0,     0,     0,     0,    58,
      59,    60,    61,    62,     0,     0,    63,    64,    65,    66,
       0,     0,     0,     0,     0,    67,     0,    68,     0,     0,
       0,     0,    69,    70,    71,     0,     0,     0,     0,     0,
       0,    72,    46,    47,    48,    49,    50,     0,     0,    51,
      52,     0,     0,     0,     0,    69,    70,    71,     0,     0,
       0,    53,    54,    55,    72,    46,    47,    48,    49,    50,
       0,     0,    51,    52,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    53,    54,    55,     0,     0,    56,
      57,     0,     0,   577,   578,   579,   580,   581,   582,   583,
       0,     0,     0,   584,   585,     0,     0,    58,    59,    60,
      61,    62,    56,    57,    63,    64,    65,    66,     0,     0,
       0,     0,     0,    67,     0,    68,     0,     0,     0,     0,
      58,    59,    60,    61,    62,     0,     0,    63,    64,    65,
      66,     0,     0,     0,     0,     0,    67,     0,    68,     0,
       0,     0,     0,    69,    70,    71,     0,     0,     0,     0,
       0,     0,    72,     0,     0,     0,     0,     0,     0,     0,
       0,   488,     0,     0,     0,     0,    69,    70,    71,    46,
      47,    48,    49,    50,     0,    72,    51,    52,     0,     0,
       0,   587,     0,   588,     0,     0,     0,     0,    53,    54,
      55,     0,    46,    47,    48,    49,    50,     0,     0,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,    54,    55,     0,     0,    56,    57,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    58,    59,    60,    61,    62,    56,
      57,    63,    64,    65,    66,     0,     0,     0,     0,     0,
      67,    82,    68,     0,     0,     0,     0,    58,    59,    60,
      61,    62,     0,     0,    63,    64,    65,    66,     0,     0,
       0,     0,     0,    67,     0,    68,     0,     0,     0,     0,
      69,    70,    71,     0,     0,     0,     0,     0,     0,    72,
       0,     0,     0,     0,     0,     0,     0,   527,     0,     0,
       0,     0,     0,    69,    70,    71,    46,    47,    48,    49,
      50,     0,    72,    51,    52,     0,     0,     0,     0,     0,
     527,     0,     0,     0,     0,    53,    54,    55,     0,     0,
       0,     0,     0,     0,     0,     0,    23,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    91,
       0,     0,     0,    56,    57,     0,   -54,   -54,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   -24,
     -24,    58,    59,    60,    61,    62,     0,     0,    63,    64,
      65,    66,     0,     0,     0,     0,     0,    67,     0,    68,
     -54,   -54,   -54,     0,   -54,   -54,     0,     0,   -54,   -54,
     -54,     0,     0,     0,    30,    31,   212,    32,    33,   -54,
     -54,    34,   -24,   213,     0,     0,     0,    69,    70,    71,
       0,     0,     0,     0,     0,     0,    72,     0,     0,     0,
     -54,   -54,   625,   -54,   -54,   -54,   -54,   -54,   -54,     0,
       0,     0,     0,   -24,    35,     0,    36,    37,    38,    39,
      40,    41,     0,     0,     0,     0,     0,     0,     0,     0,
     -54,   -54,   -54,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   -24,   -24,   -24,   173,   174,   175,   176,
     177,   178,     0,     0,   179,   180,   181,   182,   183,   184,
       0,     0,     0,     0,     0,     0,     0,   189,   190,   191,
     192,   193,   194,   173,   174,   175,   176,   177,   178,     0,
       0,   179,   180,   181,   182,   183,     0,     0,     0,     0,
       0,     0,     0,     0,   189,   190,   191,   192,   193,   194,
     173,   174,   175,   176,   177,   178,     0,     0,   179,     0,
     181,   182,   183,     0,     0,     0,     0,     0,     0,     0,
       0,   189,   190,   191,   192,   193,   194,   173,   174,   175,
     176,   177,   178,     0,     0,   179,     0,     0,   182,   183,
       0,     0,     0,     0,     0,     0,     0,     0,   189,   190,
     191,   192,   193,   194,     0,     0,     0,     0,     0,     0,
       0,   197,   198,   199,     0,   200,     0,   201,     0,   202,
     203,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   197,   198,
     199,     0,   200,     0,   201,     0,   202,   203,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   197,   198,   199,     0,   200,
       0,   201,     0,   202,   203,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   197,   198,   199,     0,   200,     0,   201,     0,
     202,   203,   173,   174,   175,   176,   177,   178,     0,     0,
     179,     0,     0,   182,   183,     0,     0,     0,     0,     0,
       0,     0,     0,   189,   190,   191,   192,   193,   194,    46,
      47,    48,    49,    50,     0,     0,   230,   231,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    53,    54,
      55,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    56,    57,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    58,   232,    60,    61,    62,     0,
       0,    63,    64,    65,    66,   233,     0,     0,   234,     0,
      67,     0,    68,     0,     0,     0,     0,     0,   198,   199,
       0,   200,     0,   201,     0,   202,   203,    46,    47,    48,
      49,    50,   147,     0,    51,    52,     0,     0,     0,     0,
      69,    70,    71,     0,     0,     0,    53,    54,    55,    72,
      46,    47,    48,    49,    50,     0,     0,    51,    52,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    53,
      54,    55,     0,     0,    56,    57,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    58,    59,    60,    61,    62,    56,    57,    63,
      64,    65,    66,   335,     0,     0,     0,     0,    67,     0,
      68,     0,     0,     0,     0,    58,   232,    60,    61,    62,
       0,     0,    63,    64,    65,    66,   233,     0,     0,   234,
       0,    67,     0,    68,     0,     0,     0,     0,    69,    70,
      71,     0,     0,     0,     0,     0,     0,    72,    46,    47,
      48,    49,    50,   147,     0,    51,    52,     0,     0,     0,
       0,    69,    70,    71,     0,     0,     0,    53,    54,    55,
      72,    46,    47,    48,    49,    50,   151,     0,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,    54,    55,     0,     0,    56,    57,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    58,    59,    60,    61,    62,    56,    57,
      63,    64,    65,    66,     0,     0,     0,     0,     0,    67,
       0,    68,     0,     0,     0,     0,    58,    59,    60,    61,
      62,     0,     0,    63,    64,    65,    66,     0,     0,     0,
       0,     0,    67,     0,    68,     0,     0,     0,     0,    69,
      70,    71,     0,     0,     0,     0,     0,     0,    72,    46,
      47,    48,    49,    50,     0,     0,    51,    52,     0,     0,
       0,     0,    69,    70,    71,     0,    51,    52,    53,    54,
      55,    72,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    56,    57,     0,     0,
     108,    30,    31,     0,    32,    33,     0,     0,    34,   109,
     110,     0,     0,     0,    58,    59,    60,    61,    62,   111,
     112,    63,    64,    65,    66,     0,     0,     0,     0,     0,
      67,     0,    68,     0,     0,     0,     0,     0,     0,     0,
      67,    35,     0,    36,    37,   113,    39,    40,    41,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      69,    70,    71,     0,     0,     0,     0,     0,     0,    72,
      83,    70,    71,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   108,    30,    31,     0,    32,    33,     0,
       0,    34,     0,   110,     0,     0,     0,     0,     0,     0,
       0,     0,   111,   112,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    67,    35,     0,    36,    37,   113,    39,
      40,    41,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    83,    70,    71
};

static const yytype_int16 yycheck[] =
{
       7,     8,     9,    10,    19,    25,    26,    27,    28,    27,
      17,    18,    19,    90,    21,     1,    27,   526,     1,   620,
     491,     1,   490,    14,    36,   497,     1,   499,   252,   779,
      56,   256,   773,   469,     0,    92,   507,   787,   788,   129,
      47,    48,    50,    51,   136,     1,    53,   759,   789,   790,
     142,   141,    59,    60,    61,    62,   806,   807,   140,   129,
     532,   135,   774,   775,   142,    72,   141,   141,   143,    76,
      96,   141,    98,    99,   100,   511,   797,   103,    60,   680,
     516,   135,   683,   804,   805,   136,   141,   141,   143,   136,
      15,    16,    17,    18,    19,    20,     6,   104,    23,   820,
     821,     0,     1,    59,   705,   141,    16,   708,   135,   134,
     546,    67,    68,   135,   141,    25,    26,    27,    28,   141,
     134,     1,    21,    22,   131,   132,   350,   129,   140,   141,
     601,   135,   733,    15,    16,   143,   140,   128,    21,    22,
     147,    21,    22,   129,   151,   261,   153,   154,   155,   156,
     157,   158,   159,   662,   134,   380,   757,   214,   142,   142,
      36,   629,   142,   141,   765,   140,   141,   141,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,   185,   186,
     187,    64,   189,   190,   191,   192,   193,   194,   195,   196,
     197,   198,   199,   135,   201,   140,    95,   204,   108,   141,
     140,   208,   209,   141,   110,   143,   322,   323,   141,   134,
     143,   136,    95,   138,   139,    95,    96,   123,    11,   129,
     141,   228,   229,   133,   141,   232,   125,   126,   127,   244,
     245,   142,    25,    26,     2,    28,     4,   244,   245,   145,
     146,   141,   125,   126,   127,   125,   126,   127,   364,   135,
     141,    44,   134,   142,   136,   141,   138,   139,   265,   266,
     769,     1,   135,     1,   140,   141,    54,   410,   141,   412,
      76,   135,   781,   782,   783,     1,     1,   141,   135,   140,
     141,    21,    22,   134,   141,   135,   293,   142,     1,    64,
      65,    66,   408,   409,   136,    21,    22,     1,   305,    92,
     210,   140,   141,   419,   136,   211,   141,   213,    21,    22,
     216,    51,   140,   141,   224,   141,   109,   110,    56,    57,
      58,    59,    60,    19,   440,    63,   140,   141,   144,   336,
     123,    56,    57,   239,    59,    60,   140,   141,    63,   140,
     141,   134,   252,   140,   141,   461,   256,    77,    78,   255,
     140,   141,   145,   146,   264,    95,   140,   141,    96,   129,
      98,    99,   100,   101,   102,   103,    68,   374,   140,    95,
     377,    96,     1,    98,    99,   100,   101,   102,   103,   386,
     387,   140,    95,   390,   142,   125,   126,   127,   140,   141,
     140,   141,    21,    22,   140,   141,     3,   135,   134,   125,
     126,   127,   143,   410,   136,   412,   413,   129,   415,   202,
     135,   129,   125,   126,   127,     1,   140,   141,   211,     1,
     213,   214,   137,   216,     1,   142,   433,   434,   129,   222,
     223,    50,    51,   510,   140,    21,    22,   230,   231,   129,
     350,   134,   449,   142,    21,    22,   239,   136,   455,   140,
     141,   458,   140,   246,   140,   141,   362,     1,   142,   479,
     143,   479,   255,   431,   432,   136,    95,   377,   135,   142,
     380,   135,     3,   383,   384,   134,   137,    21,    22,   137,
     135,   135,   135,   143,   491,   142,   396,   143,   140,   140,
     129,    21,    22,   403,   143,   143,   125,   126,   127,   143,
     507,   129,   522,   143,   522,   142,   526,   129,   526,    95,
     517,   522,   418,   140,   134,   140,   134,   524,    95,   140,
     527,   134,   134,   530,   317,    21,    22,   142,   535,   536,
     537,   538,   140,   326,   327,   140,   442,    69,   140,   125,
     126,   127,    69,   140,   140,   135,   129,   135,   125,   126,
     127,    95,    43,    15,    16,   348,   349,    19,    20,   143,
     140,    23,    77,   134,   140,    95,   135,   142,   142,   362,
      49,   586,   142,    71,   367,   142,   142,   584,   585,   586,
     140,   125,   126,   127,   142,   142,   129,   129,    70,    72,
      21,    22,   599,    71,   601,   125,   126,   127,   605,    95,
     130,    73,   140,   618,   140,   140,   143,   140,   623,   616,
       5,   618,   522,    88,   140,   143,   623,   140,   411,   143,
     214,   527,   143,   143,   631,   418,   143,   251,   133,   125,
     126,   127,   599,   362,   264,    21,    22,   418,   431,   432,
     655,   484,   662,   658,   662,   633,    77,    78,   655,   442,
     442,   658,   596,   714,   771,   777,   553,   738,    21,    22,
     530,    13,   224,   463,    95,   224,   403,   682,    14,   700,
     463,   607,   134,   605,   136,   682,   138,   139,    15,    16,
      17,    18,    19,    20,   764,   231,    23,     1,    51,    26,
      27,   706,   411,   599,   125,   126,   127,   762,    -1,   706,
      14,    15,    16,    17,    18,   720,    -1,    21,    22,    95,
      96,    -1,   618,   720,    -1,    -1,    -1,    -1,    -1,    33,
      34,    35,    21,    22,    -1,    -1,    -1,    -1,    -1,   736,
      -1,    -1,    95,    -1,   527,     0,     1,    -1,    -1,   125,
     126,   127,    -1,    -1,    -1,    -1,    -1,    61,    62,   542,
      -1,    -1,    -1,    -1,    -1,   762,    21,    22,    -1,   552,
      -1,    -1,   125,   126,   127,    79,    80,    81,    82,    83,
     777,   681,    86,    87,    88,    89,    -1,    -1,    77,   786,
     795,    95,    96,    97,    -1,    -1,    -1,    -1,   795,    -1,
      55,    56,    57,    -1,    59,    60,    95,   134,    63,   136,
      65,   138,   139,    -1,    -1,    -1,   599,    -1,    -1,    74,
      75,   125,   126,   127,   607,    -1,   609,   610,   611,    -1,
     134,    -1,    -1,    -1,    -1,   618,   125,   126,   127,    -1,
      95,    96,    -1,    98,    99,   100,   101,   102,   103,    -1,
     633,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   648,    -1,    -1,    -1,    -1,
     125,   126,   127,    15,    16,    17,    18,    19,    20,    -1,
      -1,    23,    -1,   666,    26,    27,    -1,    -1,   671,    -1,
      -1,   674,    -1,    -1,    -1,    -1,    -1,    -1,    15,    16,
      17,    18,    19,    20,    -1,    -1,    23,    24,    25,    26,
      27,    28,    29,    -1,    -1,   698,    -1,   700,    -1,    36,
      37,    38,    39,    40,    41,    -1,    -1,    -1,   711,   712,
      -1,   714,    -1,   716,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   727,   728,     0,     1,    -1,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      -1,    15,    16,    17,    18,    19,    20,    -1,    -1,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    -1,
      -1,    -1,    36,    37,    38,    39,    40,    41,   771,   131,
     132,    -1,   134,    -1,   136,    -1,   138,   139,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   128,     1,   130,   131,   132,    -1,   134,    -1,   136,
      -1,   138,   139,    -1,    -1,    14,    15,    16,    17,    18,
      84,    -1,    21,    22,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    33,    34,    35,    -1,    -1,    -1,
      -1,    -1,    -1,    42,    43,    44,    45,    46,    47,    48,
      -1,    50,    51,    52,    53,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    61,    62,   128,    -1,   130,   131,   132,    -1,
     134,    -1,   136,    -1,   138,   139,    -1,   141,    -1,   143,
      79,    80,    81,    82,    83,    -1,    -1,    86,    87,    88,
      89,    -1,    -1,    -1,    -1,    -1,    95,    -1,    97,    -1,
      -1,    -1,    -1,    -1,     1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     1,    -1,    -1,    -1,    -1,    14,    15,    16,
      17,    18,    -1,    -1,    21,    22,   125,   126,   127,    -1,
      -1,    -1,    -1,    -1,    -1,   134,    33,    34,    35,    -1,
      -1,   140,    -1,   142,   143,    42,    43,    44,    45,    46,
      47,    48,    -1,    50,    51,    52,    53,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    61,    62,    -1,    56,    57,    58,
      59,    60,    -1,    -1,    63,    -1,    -1,    66,    -1,    -1,
      -1,    -1,    79,    80,    81,    82,    83,    -1,    -1,    86,
      87,    88,    89,    -1,    -1,    -1,    -1,    -1,    95,    -1,
      97,    -1,    -1,    -1,    -1,    -1,     1,    96,    -1,    98,
      99,   100,   101,   102,   103,     1,    -1,    -1,    -1,    14,
      15,    16,    17,    18,    -1,    -1,    21,    22,   125,   126,
     127,    -1,    -1,    -1,    -1,    -1,    -1,   134,    33,    34,
      35,    -1,    -1,   140,    -1,   142,   143,    42,    43,    44,
      45,    46,    47,    48,    -1,    50,    51,    52,    53,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    61,    62,    -1,    55,
      56,    57,    -1,    59,    60,    -1,    -1,    63,    -1,    -1,
      66,    -1,    -1,    -1,    79,    80,    81,    82,    83,    -1,
      -1,    86,    87,    88,    89,    -1,    -1,    -1,    -1,    -1,
      95,    -1,    97,    -1,    -1,    -1,    -1,    -1,     1,    -1,
      96,    -1,    98,    99,   100,   101,   102,   103,     1,    -1,
      -1,    14,    15,    16,    17,    18,    -1,    -1,    21,    22,
     125,   126,   127,    -1,    -1,    -1,    -1,    -1,    -1,   134,
      33,    34,    35,    -1,    -1,   140,    -1,   142,   143,    42,
      43,    44,    45,    46,    47,    48,    -1,    50,    51,    52,
      53,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    61,    62,
      -1,    -1,    -1,    56,    57,    -1,    59,    60,    -1,    -1,
      63,    -1,    -1,    -1,    -1,    -1,    79,    80,    81,    82,
      83,    -1,    -1,    86,    87,    88,    89,    -1,    -1,    -1,
      -1,    -1,    95,    -1,    97,    -1,    -1,    -1,    -1,    -1,
       1,    -1,    -1,    96,    -1,    98,    99,   100,   101,   102,
     103,     1,    -1,    14,    15,    16,    17,    18,    -1,    -1,
      21,    22,   125,   126,   127,    -1,    -1,    -1,    -1,    -1,
      -1,   134,    33,    34,    35,    -1,    -1,   140,    -1,   142,
     143,    42,    43,    44,    45,    46,    47,    48,    -1,    -1,
      -1,    52,    53,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      61,    62,    -1,    -1,    -1,    -1,    56,    57,    -1,    59,
      60,    -1,    -1,    63,    -1,    -1,    -1,    -1,    79,    80,
      81,    82,    83,    -1,    -1,    86,    87,    88,    89,    -1,
      -1,    -1,    -1,    -1,    95,    -1,    97,    -1,    -1,    -1,
      -1,    -1,     1,    -1,    -1,    -1,    96,    -1,    98,    99,
     100,   101,   102,   103,     1,    14,    15,    16,    17,    18,
      -1,    -1,    21,    22,   125,   126,   127,    -1,    -1,    -1,
      -1,    -1,    -1,   134,    33,    34,    35,    -1,    -1,   140,
      -1,   142,   143,    42,    43,    44,    45,    46,    47,    48,
      -1,    -1,    -1,    52,    53,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    61,    62,    -1,    -1,    -1,    -1,    -1,    56,
      57,    -1,    59,    60,    -1,    -1,    63,    -1,    -1,    -1,
      79,    80,    81,    82,    83,    -1,    -1,    86,    87,    88,
      89,    -1,    -1,    -1,    -1,    -1,    95,    -1,    97,    -1,
      -1,    -1,    -1,    -1,     1,    -1,    -1,    -1,    -1,    96,
      -1,    98,    99,   100,   101,   102,   103,    14,    15,    16,
      17,    18,    -1,    -1,    21,    22,   125,   126,   127,    -1,
      -1,    -1,    -1,    -1,    -1,   134,    33,    34,    35,    -1,
      -1,   140,    -1,   142,   143,    42,    43,    44,    45,    46,
      47,    48,    -1,    -1,    -1,    52,    53,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    61,    62,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    79,    80,    81,    82,    83,    -1,    85,    86,
      87,    88,    89,    -1,    -1,    -1,    -1,    -1,    95,    -1,
      97,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    15,    16,    17,    18,    19,    20,    -1,
      -1,    23,    -1,    -1,    26,    27,    -1,    -1,   125,   126,
     127,    -1,    -1,    -1,    36,    37,    -1,   134,    40,    41,
      -1,    -1,    -1,   140,    -1,   142,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    -1,    15,    16,
      17,    18,    19,    20,    -1,    -1,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    -1,    -1,    -1,    36,
      37,    38,    39,    40,    41,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    -1,    15,    16,    17,
      18,    19,    20,    -1,    -1,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    -1,    -1,    -1,    36,    37,
      38,    39,    40,    41,    -1,    -1,    -1,    84,    -1,   131,
     132,    -1,   134,    -1,   136,    -1,   138,   139,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      56,    57,    -1,    59,    60,    -1,    -1,    63,    56,    57,
      -1,    59,    60,    -1,    -1,    63,    84,    -1,    -1,    -1,
      -1,   128,    -1,   130,   131,   132,    -1,   134,    -1,   136,
      -1,   138,   139,    -1,    -1,    -1,   143,    -1,    -1,    -1,
      96,    -1,    98,    99,   100,   101,   102,   103,    96,    -1,
      98,    99,   100,   101,   102,   103,    -1,    -1,    -1,    -1,
     128,    -1,   130,   131,   132,    -1,   134,    -1,   136,    -1,
     138,   139,    -1,    -1,    -1,   143,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,   143,    15,    16,
      17,    18,    19,    20,    -1,   143,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    -1,    -1,    -1,    36,
      37,    38,    39,    40,    41,     1,    -1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    -1,    15,
      16,    17,    18,    19,    20,    -1,    -1,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    -1,    -1,    -1,
      36,    37,    38,    39,    40,    41,     1,    84,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    -1,    -1,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    -1,    -1,
      -1,    36,    37,    38,    39,    40,    41,    -1,    84,    -1,
      -1,   128,    -1,   130,   131,   132,    -1,   134,    -1,   136,
      -1,   138,   139,    -1,    -1,    -1,   143,    -1,    55,    56,
      57,    -1,    59,    60,    -1,    -1,    63,    56,    57,    -1,
      59,    60,    69,    -1,    63,    -1,    -1,    -1,    -1,    84,
      -1,    -1,   128,    -1,   130,   131,   132,    -1,   134,    -1,
     136,    -1,   138,   139,   140,    -1,    -1,    -1,    -1,    96,
      -1,    98,    99,   100,   101,   102,   103,    96,    -1,    98,
      99,   100,   101,   102,   103,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   128,    -1,   130,   131,   132,    -1,   134,
      -1,   136,    -1,   138,   139,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,   135,    15,    16,    17,
      18,    19,    20,    -1,    -1,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    -1,    -1,    -1,    36,    37,
      38,    39,    40,    41,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    13,    -1,    15,    16,    17,    18,
      19,    20,    -1,    -1,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    -1,    -1,    -1,    36,    37,    38,
      39,    40,    41,    -1,    -1,    -1,    84,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    55,    56,    57,    -1,    59,    60,    -1,    -1,    63,
      56,    57,    58,    59,    60,    -1,    -1,    63,    -1,    65,
      56,    57,    58,    59,    60,    84,    -1,    63,    -1,    -1,
     128,    -1,   130,   131,   132,    -1,   134,    -1,   136,    -1,
     138,   139,    96,   141,    98,    99,   100,   101,   102,   103,
      96,    -1,    98,    99,   100,   101,   102,   103,    -1,    -1,
      96,    -1,    98,    99,   100,   101,   102,   103,    -1,   128,
      -1,   130,   131,   132,    -1,   134,    -1,   136,    -1,   138,
     139,    -1,   141,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    -1,    15,    16,    17,    18,    19,
      20,    -1,    -1,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    -1,    -1,    -1,    36,    37,    38,    39,
      40,    41,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    -1,    15,    16,    17,    18,    19,    20,
      -1,    -1,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    -1,    -1,    -1,    36,    37,    38,    39,    40,
      41,    -1,    -1,    -1,    84,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      56,    57,    58,    59,    60,    -1,    -1,    63,    56,    57,
      -1,    59,    60,    -1,    -1,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    84,    -1,    -1,    -1,    -1,   128,    -1,
     130,   131,   132,    -1,   134,    -1,   136,    -1,   138,   139,
      96,   141,    98,    99,   100,   101,   102,   103,    96,    -1,
      98,    99,   100,   101,   102,   103,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   128,   129,   130,
     131,   132,    -1,   134,    -1,   136,    -1,   138,   139,   140,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    -1,    15,    16,    17,    18,    19,    20,    -1,    -1,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      -1,    -1,    -1,    36,    37,    38,    39,    40,    41,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      -1,    15,    16,    17,    18,    19,    20,    -1,    -1,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    -1,
      -1,    -1,    36,    37,    38,    39,    40,    41,    -1,    -1,
      -1,    84,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    -1,    -1,   128,    -1,   130,   131,   132,
      -1,   134,    -1,   136,    -1,   138,   139,   140,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   128,    -1,   130,   131,   132,    -1,
     134,    -1,   136,    -1,   138,   139,   140,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    -1,    15,
      16,    17,    18,    19,    20,    -1,    -1,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    -1,    -1,    -1,
      36,    37,    38,    39,    40,    41,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    -1,    15,    16,
      17,    18,    19,    20,    -1,    -1,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    -1,    -1,    -1,    36,
      37,    38,    39,    40,    41,    -1,    -1,    -1,    84,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,
      -1,    -1,   128,    -1,   130,   131,   132,    -1,   134,    -1,
     136,    -1,   138,   139,   140,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   128,    -1,   130,   131,   132,    -1,   134,    -1,   136,
      -1,   138,   139,   140,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    13,    -1,    15,    16,    17,    18,
      19,    20,    -1,    -1,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    -1,    -1,    -1,    36,    37,    38,
      39,    40,    41,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    -1,    15,    16,    17,    18,    19,
      20,    -1,    -1,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    -1,    -1,    -1,    36,    37,    38,    39,
      40,    41,    -1,    -1,    -1,    84,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    84,    -1,    -1,   126,   127,   128,
      -1,   130,   131,   132,    -1,   134,    -1,   136,    -1,   138,
     139,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   126,   127,   128,    -1,
     130,   131,   132,    -1,   134,    -1,   136,    -1,   138,   139,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    -1,    15,    16,    17,    18,    19,    20,    -1,    -1,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      -1,    -1,    -1,    36,    37,    38,    39,    40,    41,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      -1,    15,    16,    17,    18,    19,    20,    -1,    -1,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    -1,
      -1,    -1,    36,    37,    38,    39,    40,    41,    -1,    -1,
      -1,    84,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    -1,    -1,   128,    -1,   130,   131,   132,
      -1,   134,   135,   136,    -1,   138,   139,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   128,    -1,   130,   131,   132,    -1,
     134,    -1,   136,   137,   138,   139,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    -1,    15,    16,
      17,    18,    19,    20,    -1,    -1,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    -1,    -1,    -1,    36,
      37,    38,    39,    40,    41,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    -1,    15,    16,    17,
      18,    19,    20,    -1,    -1,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    -1,    -1,    -1,    36,    37,
      38,    39,    40,    41,    -1,    -1,    -1,    84,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,    -1,
      -1,   128,   129,   130,   131,   132,    -1,   134,    -1,   136,
      -1,   138,   139,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     128,    -1,   130,   131,   132,    -1,   134,    -1,   136,   137,
     138,   139,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    -1,    15,    16,    17,    18,    19,    20,
      -1,    -1,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    -1,    -1,    -1,    36,    37,    38,    39,    40,
      41,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    -1,    15,    16,    17,    18,    19,    20,    -1,
      -1,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    -1,    -1,    -1,    36,    37,    38,    39,    40,    41,
      -1,    -1,    -1,    84,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    84,    -1,    -1,    -1,    -1,   128,    -1,   130,
     131,   132,    -1,   134,    -1,   136,   137,   138,   139,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   128,    -1,   130,   131,
     132,    -1,   134,    -1,   136,   137,   138,   139,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    -1,
      15,    16,    17,    18,    19,    20,    -1,    -1,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    -1,    -1,
      -1,    36,    37,    38,    39,    40,    41,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    -1,    15,
      16,    17,    18,    19,    20,    -1,    -1,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    -1,    -1,    -1,
      36,    37,    38,    39,    40,    41,    -1,    -1,    -1,    84,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,
      -1,    -1,    -1,   128,    -1,   130,   131,   132,    -1,   134,
      -1,   136,   137,   138,   139,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   128,    -1,   130,   131,   132,    -1,   134,    -1,
     136,   137,   138,   139,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    13,    -1,    15,    16,    17,    18,
      19,    20,    -1,    -1,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    -1,    -1,    -1,    36,    37,    38,
      39,    40,    41,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    -1,    15,    16,    17,    18,    19,
      20,    -1,    -1,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    -1,    -1,    -1,    36,    37,    38,    39,
      40,    41,    -1,    -1,    -1,    84,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    84,    -1,    -1,    -1,    -1,   128,
      -1,   130,   131,   132,    -1,   134,    -1,   136,   137,   138,
     139,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   128,    -1,
     130,   131,   132,    -1,   134,    -1,   136,   137,   138,   139,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    -1,    15,    16,    17,    18,    19,    20,    -1,    -1,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      -1,    -1,    -1,    36,    37,    38,    39,    40,    41,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      -1,    15,    16,    17,    18,    19,    20,    -1,    -1,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    -1,
      -1,    -1,    36,    37,    38,    39,    40,    41,    -1,    -1,
      -1,    84,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      84,    -1,    -1,    -1,    -1,   128,    -1,   130,   131,   132,
      -1,   134,    -1,   136,   137,   138,   139,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   128,    -1,   130,   131,   132,    -1,
     134,   135,   136,    -1,   138,   139,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    -1,    15,    16,
      17,    18,    19,    20,    -1,    -1,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    -1,    -1,    -1,    36,
      37,    38,    39,    40,    41,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    -1,    15,    16,    17,
      18,    19,    20,    -1,    -1,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    -1,    -1,    -1,    36,    37,
      38,    39,    40,    41,    -1,    -1,    -1,    84,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,    -1,
      -1,   128,    -1,   130,   131,   132,    -1,   134,    -1,   136,
     137,   138,   139,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     128,    -1,   130,   131,   132,    -1,   134,    -1,   136,   137,
     138,   139,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    -1,    15,    16,    17,    18,    19,    20,
      -1,    -1,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    -1,    -1,    -1,    36,    37,    38,    39,    40,
      41,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    -1,    15,    16,    17,    18,    19,    20,    -1,
      -1,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    -1,    -1,    -1,    36,    37,    38,    39,    40,    41,
      -1,    -1,    -1,    84,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    84,    -1,    -1,    -1,    -1,   128,   129,   130,
     131,   132,    -1,   134,    -1,   136,    -1,   138,   139,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   128,    -1,   130,   131,
     132,    -1,   134,    -1,   136,    -1,   138,   139,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    -1,
      15,    16,    17,    18,    19,    20,    -1,    -1,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    -1,    -1,
      -1,    36,    37,    38,    39,    40,    41,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    -1,    15,
      16,    17,    18,    19,    20,    -1,    -1,    23,    24,    25,
      26,    27,    28,    29,    30,    -1,    -1,    -1,    -1,    -1,
      36,    37,    38,    39,    40,    41,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    -1,    15,    16,
      17,    18,    19,    20,    -1,    -1,    23,    24,    25,    26,
      27,    28,    29,    -1,    -1,    -1,    -1,    -1,    -1,    36,
      37,    38,    39,    40,    41,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   128,    -1,   130,   131,   132,    -1,   134,
      -1,   136,    -1,   138,   139,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     1,   128,    -1,   130,   131,   132,    -1,   134,    -1,
     136,    -1,   138,   139,    14,    15,    16,    17,    18,    -1,
      -1,    21,    22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    33,    34,    35,    -1,    -1,    -1,    -1,
      -1,   128,    -1,   130,   131,   132,    -1,   134,    -1,   136,
      -1,   138,   139,    -1,    -1,    -1,    56,    57,    -1,    59,
      60,    61,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    79,
      80,    81,    82,    83,    -1,    -1,    86,    87,    88,    89,
      -1,    -1,    -1,    -1,    -1,    95,    96,    97,    98,    99,
     100,   101,   102,   103,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   125,   126,   127,    14,    15,
      16,    17,    18,    -1,   134,    21,    22,    -1,     1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    33,    34,    35,
      -1,    14,    15,    16,    17,    18,    -1,    -1,    21,    22,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      33,    34,    35,    -1,    -1,    61,    62,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    79,    80,    81,    82,    83,    61,    62,
      86,    87,    88,    89,    -1,    -1,    -1,    -1,    -1,    95,
      -1,    97,    -1,    -1,    -1,    -1,    79,    80,    81,    82,
      83,    -1,    -1,    86,    87,    88,    89,    -1,    -1,    -1,
      -1,    -1,    95,    -1,    97,    -1,     1,    -1,    -1,   125,
     126,   127,    -1,    -1,    -1,    -1,    -1,    -1,   134,    14,
      15,    16,    17,    18,    -1,    -1,    21,    22,    -1,     1,
      -1,    -1,   125,   126,   127,    -1,    -1,    -1,    33,    34,
      35,   134,    14,    15,    16,    17,    18,    -1,    -1,    21,
      22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    33,    34,    35,    -1,    -1,    61,    62,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    79,    80,    81,    82,    83,    61,
      62,    86,    87,    88,    89,    -1,    -1,    -1,    -1,    -1,
      95,    -1,    97,    -1,    -1,    -1,    -1,    79,    80,    81,
      82,    83,    -1,    -1,    86,    87,    88,    89,    -1,    -1,
      -1,    -1,    -1,    95,    -1,    97,    -1,     1,    -1,    -1,
     125,   126,   127,    -1,    -1,    -1,    -1,    -1,    -1,   134,
      14,    15,    16,    17,    18,    -1,    -1,    21,    22,    -1,
       1,    -1,    -1,   125,   126,   127,    -1,    -1,    -1,    33,
      34,    35,   134,    14,    15,    16,    17,    18,    -1,    -1,
      21,    22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    33,    34,    35,    -1,    -1,    61,    62,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    79,    80,    81,    82,    83,
      61,    62,    86,    87,    88,    89,    -1,    -1,    -1,    -1,
      -1,    95,    -1,    97,    -1,    -1,    -1,    -1,    79,    80,
      81,    82,    83,    -1,    -1,    86,    87,    88,    89,    -1,
      -1,    -1,    -1,    -1,    95,    -1,    97,    -1,     1,    -1,
      -1,   125,   126,   127,    -1,    -1,    -1,    -1,    -1,    -1,
     134,    14,    15,    16,    17,    18,    -1,    -1,    21,    22,
      -1,     1,    -1,    -1,   125,   126,   127,    -1,    -1,    -1,
      33,    34,    35,   134,    14,    15,    16,    17,    18,    -1,
      -1,    21,    22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    33,    34,    35,    -1,    -1,    61,    62,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    79,    80,    81,    82,
      83,    61,    62,    86,    87,    88,    89,    -1,    -1,    -1,
      -1,    -1,    95,    -1,    97,    -1,    -1,    -1,    -1,    79,
      80,    81,    82,    83,    -1,    -1,    86,    87,    88,    89,
      -1,    -1,    -1,    -1,    -1,    95,    -1,    97,    -1,    -1,
      -1,    -1,   125,   126,   127,    -1,    -1,    -1,    -1,    -1,
      -1,   134,    14,    15,    16,    17,    18,    -1,    -1,    21,
      22,    -1,    -1,    -1,    -1,   125,   126,   127,    -1,    -1,
      -1,    33,    34,    35,   134,    14,    15,    16,    17,    18,
      -1,    -1,    21,    22,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    33,    34,    35,    -1,    -1,    61,
      62,    -1,    -1,    42,    43,    44,    45,    46,    47,    48,
      -1,    -1,    -1,    52,    53,    -1,    -1,    79,    80,    81,
      82,    83,    61,    62,    86,    87,    88,    89,    -1,    -1,
      -1,    -1,    -1,    95,    -1,    97,    -1,    -1,    -1,    -1,
      79,    80,    81,    82,    83,    -1,    -1,    86,    87,    88,
      89,    -1,    -1,    -1,    -1,    -1,    95,    -1,    97,    -1,
      -1,    -1,    -1,   125,   126,   127,    -1,    -1,    -1,    -1,
      -1,    -1,   134,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   143,    -1,    -1,    -1,    -1,   125,   126,   127,    14,
      15,    16,    17,    18,    -1,   134,    21,    22,    -1,    -1,
      -1,   140,    -1,   142,    -1,    -1,    -1,    -1,    33,    34,
      35,    -1,    14,    15,    16,    17,    18,    -1,    -1,    21,
      22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    33,    34,    35,    -1,    -1,    61,    62,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    79,    80,    81,    82,    83,    61,
      62,    86,    87,    88,    89,    -1,    -1,    -1,    -1,    -1,
      95,    96,    97,    -1,    -1,    -1,    -1,    79,    80,    81,
      82,    83,    -1,    -1,    86,    87,    88,    89,    -1,    -1,
      -1,    -1,    -1,    95,    -1,    97,    -1,    -1,    -1,    -1,
     125,   126,   127,    -1,    -1,    -1,    -1,    -1,    -1,   134,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   142,    -1,    -1,
      -1,    -1,    -1,   125,   126,   127,    14,    15,    16,    17,
      18,    -1,   134,    21,    22,    -1,    -1,    -1,    -1,    -1,
     142,    -1,    -1,    -1,    -1,    33,    34,    35,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,
      -1,    -1,    -1,    61,    62,    -1,    21,    22,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    21,
      22,    79,    80,    81,    82,    83,    -1,    -1,    86,    87,
      88,    89,    -1,    -1,    -1,    -1,    -1,    95,    -1,    97,
      55,    56,    57,    -1,    59,    60,    -1,    -1,    63,    64,
      65,    -1,    -1,    -1,    56,    57,    58,    59,    60,    74,
      75,    63,    64,    65,    -1,    -1,    -1,   125,   126,   127,
      -1,    -1,    -1,    -1,    -1,    -1,   134,    -1,    -1,    -1,
      95,    96,   140,    98,    99,   100,   101,   102,   103,    -1,
      -1,    -1,    -1,    95,    96,    -1,    98,    99,   100,   101,
     102,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     125,   126,   127,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   125,   126,   127,    15,    16,    17,    18,
      19,    20,    -1,    -1,    23,    24,    25,    26,    27,    28,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    36,    37,    38,
      39,    40,    41,    15,    16,    17,    18,    19,    20,    -1,
      -1,    23,    24,    25,    26,    27,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    36,    37,    38,    39,    40,    41,
      15,    16,    17,    18,    19,    20,    -1,    -1,    23,    -1,
      25,    26,    27,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    36,    37,    38,    39,    40,    41,    15,    16,    17,
      18,    19,    20,    -1,    -1,    23,    -1,    -1,    26,    27,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    36,    37,
      38,    39,    40,    41,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   130,   131,   132,    -1,   134,    -1,   136,    -1,   138,
     139,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   130,   131,
     132,    -1,   134,    -1,   136,    -1,   138,   139,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   130,   131,   132,    -1,   134,
      -1,   136,    -1,   138,   139,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   130,   131,   132,    -1,   134,    -1,   136,    -1,
     138,   139,    15,    16,    17,    18,    19,    20,    -1,    -1,
      23,    -1,    -1,    26,    27,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    36,    37,    38,    39,    40,    41,    14,
      15,    16,    17,    18,    -1,    -1,    21,    22,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    33,    34,
      35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    61,    62,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    79,    80,    81,    82,    83,    -1,
      -1,    86,    87,    88,    89,    90,    -1,    -1,    93,    -1,
      95,    -1,    97,    -1,    -1,    -1,    -1,    -1,   131,   132,
      -1,   134,    -1,   136,    -1,   138,   139,    14,    15,    16,
      17,    18,    19,    -1,    21,    22,    -1,    -1,    -1,    -1,
     125,   126,   127,    -1,    -1,    -1,    33,    34,    35,   134,
      14,    15,    16,    17,    18,    -1,    -1,    21,    22,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    33,
      34,    35,    -1,    -1,    61,    62,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    79,    80,    81,    82,    83,    61,    62,    86,
      87,    88,    89,    90,    -1,    -1,    -1,    -1,    95,    -1,
      97,    -1,    -1,    -1,    -1,    79,    80,    81,    82,    83,
      -1,    -1,    86,    87,    88,    89,    90,    -1,    -1,    93,
      -1,    95,    -1,    97,    -1,    -1,    -1,    -1,   125,   126,
     127,    -1,    -1,    -1,    -1,    -1,    -1,   134,    14,    15,
      16,    17,    18,    19,    -1,    21,    22,    -1,    -1,    -1,
      -1,   125,   126,   127,    -1,    -1,    -1,    33,    34,    35,
     134,    14,    15,    16,    17,    18,    19,    -1,    21,    22,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      33,    34,    35,    -1,    -1,    61,    62,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    79,    80,    81,    82,    83,    61,    62,
      86,    87,    88,    89,    -1,    -1,    -1,    -1,    -1,    95,
      -1,    97,    -1,    -1,    -1,    -1,    79,    80,    81,    82,
      83,    -1,    -1,    86,    87,    88,    89,    -1,    -1,    -1,
      -1,    -1,    95,    -1,    97,    -1,    -1,    -1,    -1,   125,
     126,   127,    -1,    -1,    -1,    -1,    -1,    -1,   134,    14,
      15,    16,    17,    18,    -1,    -1,    21,    22,    -1,    -1,
      -1,    -1,   125,   126,   127,    -1,    21,    22,    33,    34,
      35,   134,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    61,    62,    -1,    -1,
      55,    56,    57,    -1,    59,    60,    -1,    -1,    63,    64,
      65,    -1,    -1,    -1,    79,    80,    81,    82,    83,    74,
      75,    86,    87,    88,    89,    -1,    -1,    -1,    -1,    -1,
      95,    -1,    97,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      95,    96,    -1,    98,    99,   100,   101,   102,   103,    21,
      22,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     125,   126,   127,    -1,    -1,    -1,    -1,    -1,    -1,   134,
     125,   126,   127,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    55,    56,    57,    -1,    59,    60,    -1,
      -1,    63,    -1,    65,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    74,    75,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    95,    96,    -1,    98,    99,   100,   101,
     102,   103,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   125,   126,   127
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   146,     1,   147,   163,   163,   199,   163,   147,
      56,    57,    59,    60,    63,    96,    98,    99,   100,   101,
     102,   103,   169,   170,   187,   195,    14,    15,    16,    17,
      18,    21,    22,    33,    34,    35,    61,    62,    79,    80,
      81,    82,    83,    86,    87,    88,    89,    95,    97,   125,
     126,   127,   134,   189,   243,   250,   252,   243,   216,   243,
     242,   243,    96,   125,   188,   189,   213,   254,   255,   255,
     276,     1,   148,    58,   187,   268,   269,   272,   243,   280,
     243,   288,   242,   289,   290,   243,     0,   140,    55,    64,
      65,    74,    75,   100,   149,   152,   153,   158,   164,   165,
     166,   171,   183,   187,   189,   196,   166,   171,   183,   142,
      60,   136,   136,   141,   130,   189,    56,    96,    98,    99,
     100,   103,   243,   243,   243,   134,   134,    19,   243,   243,
     243,    19,   243,   142,   249,   249,   249,   249,   136,   142,
       1,   243,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    15,    16,    17,    18,    19,    20,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    36,
      37,    38,    39,    40,    41,    84,   128,   130,   131,   132,
     134,   136,   138,   139,   251,   243,     1,    14,   128,   141,
     129,   141,    58,    65,   148,   171,   187,   256,   261,   256,
     140,   149,   273,   270,   140,   271,   141,     1,   141,   141,
      21,    22,    80,    90,    93,   243,   293,   294,     1,   187,
       1,   159,   189,   188,   142,   142,    54,    76,   161,   172,
     173,   188,   134,   151,     1,   187,   190,   191,   243,   243,
     170,   189,   179,   180,   142,   136,   136,   188,   188,   243,
     243,   243,   243,   243,   243,   243,   243,   243,   135,   135,
     243,   243,   243,   243,   243,   243,   243,   243,   243,   243,
     243,   243,   243,   246,   243,   243,   243,   243,   243,   243,
     243,   243,   243,   243,   243,   244,   245,     1,   243,   189,
     243,    14,   128,   243,   187,   188,     1,   257,     1,   188,
     152,   188,   189,   189,   269,   272,   141,   274,   141,   243,
     243,   189,   291,   292,   292,    90,   129,    19,    68,   295,
     144,   140,   184,   185,   188,   140,    36,   140,   141,   160,
     134,   168,   242,   242,     1,    51,   154,   156,   157,   189,
     142,   140,   141,   168,   174,   135,   169,     3,   143,   188,
     192,   193,   143,   191,   141,   137,   179,   136,   181,     1,
     190,   243,   243,   129,   129,   143,   126,   127,   143,   243,
     129,   243,   253,     1,   137,   137,   129,   140,   189,   258,
     259,   142,     1,   134,   142,   267,   179,   179,   189,   189,
     129,   141,   129,   129,   243,   134,   296,   140,   141,   186,
     189,   189,     1,   135,   169,   142,   143,   143,   140,    36,
     140,   141,   155,   136,   162,   173,   142,   179,   135,   189,
     194,   140,   141,   243,     1,   187,   243,   143,   143,   141,
     137,   187,   187,   243,   243,   136,   243,   135,   141,   135,
     187,   260,   140,   141,   265,   142,     1,   135,   268,   266,
       1,   142,   179,   179,   293,   291,   293,   243,   243,   225,
     185,   179,   135,   135,   197,   156,   156,   243,   143,   243,
     167,     3,   175,   134,   179,   193,   137,   137,   137,   137,
     243,   135,   135,   137,   137,   243,   243,   179,   259,   275,
     276,   264,   135,   135,   275,   142,   262,   141,    69,   171,
     183,   198,   199,   137,   129,   140,   225,   142,   176,   243,
     150,   181,   182,   181,   137,   247,   248,   126,   127,   176,
     143,     1,    66,   277,   275,   143,   263,   275,   243,   226,
     143,     1,    66,   200,   243,   226,   176,   177,   178,   188,
     189,   253,   181,   243,   243,   243,   243,   140,   189,   278,
     279,   218,   143,   275,   143,   135,     1,    42,    43,    44,
      45,    46,    47,    48,    52,    53,    85,   140,   142,   223,
     227,   243,   140,   189,   201,   202,   218,   140,   143,   141,
     143,   129,   135,   137,   137,   142,   140,   141,     1,    59,
      67,    68,   203,   219,   220,   143,   129,   140,   134,   134,
     231,   140,   140,   134,   134,   140,   243,   243,   242,   224,
     140,   142,   140,   141,   203,   178,   176,   140,   280,   279,
     140,     1,   189,   222,     1,   189,   221,   189,    69,   281,
     243,     1,   188,   242,     1,   230,   227,   242,   232,   140,
     140,   140,   225,     1,   243,   202,    69,   204,   143,   140,
     140,   141,   140,   140,   141,   140,     1,   189,   282,   283,
     135,   129,   140,   135,   242,    43,   135,   242,   226,   143,
     143,     1,   189,   205,   206,   189,   189,   140,    77,   140,
     141,   227,   187,   242,   227,   135,   134,   233,   135,   143,
     140,    77,    78,   140,   141,   189,    77,   283,   285,   135,
     140,   227,   242,   142,   227,   189,   189,    77,    78,   206,
     209,   142,   189,   229,   242,   135,    50,    51,   236,   237,
      49,   234,   142,   142,   189,   189,   284,   142,   227,   135,
     140,     1,   243,     1,   129,   143,   237,   235,   207,   208,
     142,   142,    71,   287,   286,   228,   129,   129,   129,   240,
     227,    70,   212,   212,   210,   211,   288,    72,   215,   287,
     227,   239,   238,   241,   226,   213,    71,   214,   214,   212,
     212,     1,   140,     1,   216,    73,   217,   215,   226,   226,
     226,   140,     1,   243,   215,   215,   214,   214,   140,   140,
     140,     1,   242,   143,   217,   140,     1,   140,   217,   217,
     215,   215,   140,   140,   143,   140,   143,   143,   217,   217,
     143,   143
};

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
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
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
    while (YYID (0))
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
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, Location); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (yylocationp);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, yylocationp)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yylsp, yyrule)
    YYSTYPE *yyvsp;
    YYLTYPE *yylsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       , &(yylsp[(yyi + 1) - (yynrhs)])		       );
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, yylsp, Rule); \
} while (YYID (0))

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
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
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
  YYUSE (yyvaluep);
  YYUSE (yylocationp);

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
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
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
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;

  /* The location stack.  */
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[2];

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

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

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;
	YYLTYPE *yyls1 = yyls;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
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
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
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

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

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

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;
  *++yylsp = yylloc;
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

  /* Default location.  */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 202 "parser.yy"
    { CALL((yylsp[(2) - (2)]), (yylsp[(2) - (2)]), done()); }
    break;

  case 3:
#line 203 "parser.yy"
    { }
    break;

  case 4:
#line 204 "parser.yy"
    { }
    break;

  case 5:
#line 205 "parser.yy"
    { }
    break;

  case 6:
#line 206 "parser.yy"
    { }
    break;

  case 7:
#line 207 "parser.yy"
    { }
    break;

  case 8:
#line 208 "parser.yy"
    { }
    break;

  case 9:
#line 209 "parser.yy"
    { }
    break;

  case 10:
#line 210 "parser.yy"
    { CALL((yylsp[(2) - (2)]), (yylsp[(2) - (2)]), procGuard()); }
    break;

  case 11:
#line 211 "parser.yy"
    { }
    break;

  case 12:
#line 212 "parser.yy"
    { CALL((yylsp[(2) - (2)]), (yylsp[(2) - (2)]), procUpdate()); }
    break;

  case 13:
#line 213 "parser.yy"
    { CALL((yylsp[(2) - (2)]), (yylsp[(2) - (2)]), done()); }
    break;

  case 14:
#line 214 "parser.yy"
    { }
    break;

  case 15:
#line 215 "parser.yy"
    { }
    break;

  case 16:
#line 216 "parser.yy"
    { }
    break;

  case 17:
#line 217 "parser.yy"
    { }
    break;

  case 18:
#line 218 "parser.yy"
    { }
    break;

  case 19:
#line 219 "parser.yy"
    { CALL((yylsp[(2) - (2)]), (yylsp[(2) - (2)]), procGuard()); }
    break;

  case 20:
#line 220 "parser.yy"
    { CALL((yylsp[(2) - (2)]), (yylsp[(2) - (2)]), procUpdate()); }
    break;

  case 21:
#line 221 "parser.yy"
    {}
    break;

  case 22:
#line 222 "parser.yy"
    {}
    break;

  case 27:
#line 236 "parser.yy"
    {
          CALL((yylsp[(1) - (5)]), (yylsp[(4) - (5)]), instantiationBegin((yyvsp[(1) - (5)].string), (yyvsp[(2) - (5)].number), (yyvsp[(4) - (5)].string)));
        }
    break;

  case 28:
#line 238 "parser.yy"
    {
          CALL((yylsp[(1) - (9)]), (yylsp[(9) - (9)]), instantiationEnd((yyvsp[(1) - (9)].string), (yyvsp[(2) - (9)].number), (yyvsp[(4) - (9)].string), (yyvsp[(7) - (9)].number)));
        }
    break;

  case 29:
#line 243 "parser.yy"
    { (yyval.number) = 0; }
    break;

  case 30:
#line 245 "parser.yy"
    {
        	(yyval.number) = 0; 
          }
    break;

  case 31:
#line 249 "parser.yy"
    {
        	(yyval.number) = (yyvsp[(2) - (3)].number);
          }
    break;

  case 38:
#line 267 "parser.yy"
    { CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), incChanPriority()); }
    break;

  case 39:
#line 271 "parser.yy"
    { CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), chanPriority()); }
    break;

  case 40:
#line 272 "parser.yy"
    { CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), defaultChanPriority()); }
    break;

  case 41:
#line 276 "parser.yy"
    { CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), exprId((yyvsp[(1) - (1)].string))); }
    break;

  case 42:
#line 277 "parser.yy"
    { CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), exprArray()); }
    break;

  case 45:
#line 286 "parser.yy"
    { CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), process((yyvsp[(1) - (1)].string))); }
    break;

  case 46:
#line 287 "parser.yy"
    { CALL((yylsp[(3) - (3)]), (yylsp[(3) - (3)]), process((yyvsp[(3) - (3)].string))); }
    break;

  case 47:
#line 288 "parser.yy"
    { CALL((yylsp[(3) - (3)]), (yylsp[(3) - (3)]), process((yyvsp[(3) - (3)].string))); }
    break;

  case 48:
#line 292 "parser.yy"
    { CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), incProcPriority()); }
    break;

  case 52:
#line 300 "parser.yy"
    {
            CALL((yylsp[(2) - (5)]), (yylsp[(4) - (5)]), declProgress(true));
        }
    break;

  case 53:
#line 303 "parser.yy"
    {
            CALL((yylsp[(2) - (3)]), (yylsp[(2) - (3)]), declProgress(false));
        }
    break;

  case 64:
#line 320 "parser.yy"
    { CALL((yylsp[(3) - (4)]), (yylsp[(3) - (4)]), beforeUpdate()); }
    break;

  case 65:
#line 322 "parser.yy"
    { CALL((yylsp[(3) - (4)]), (yylsp[(3) - (4)]), afterUpdate()); }
    break;

  case 66:
#line 329 "parser.yy"
    { 
          CALL((yylsp[(1) - (4)]), (yylsp[(2) - (4)]), declFuncBegin((yyvsp[(2) - (4)].string)));
        }
    break;

  case 67:
#line 331 "parser.yy"
    {
          CALL((yylsp[(8) - (8)]), (yylsp[(8) - (8)]), declFuncEnd());
        }
    break;

  case 71:
#line 342 "parser.yy"
    { (yyval.number) = 1; }
    break;

  case 72:
#line 343 "parser.yy"
    { (yyval.number) = (yyvsp[(1) - (3)].number)+1; }
    break;

  case 73:
#line 347 "parser.yy"
    {
          CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), declParameter((yyvsp[(3) - (4)].string), true));
        }
    break;

  case 74:
#line 350 "parser.yy"
    {
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), declParameter((yyvsp[(2) - (3)].string), false));
        }
    break;

  case 75:
#line 356 "parser.yy"
    { 
            CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), typePop());
        }
    break;

  case 78:
#line 367 "parser.yy"
    {
            CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeDuplicate());
        }
    break;

  case 79:
#line 369 "parser.yy"
    { 
            CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), declVar((yyvsp[(1) - (4)].string), (yyvsp[(4) - (4)].flag)));
        }
    break;

  case 80:
#line 375 "parser.yy"
    { (yyval.flag) = false; }
    break;

  case 81:
#line 376 "parser.yy"
    { (yyval.flag) = true; }
    break;

  case 83:
#line 381 "parser.yy"
    {	
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), declInitialiserList((yyvsp[(2) - (3)].number)));
        }
    break;

  case 84:
#line 387 "parser.yy"
    { (yyval.number) = 1; }
    break;

  case 85:
#line 388 "parser.yy"
    { (yyval.number) = (yyvsp[(1) - (3)].number)+1; }
    break;

  case 86:
#line 392 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), declFieldInit((yyvsp[(1) - (3)].string)));
        }
    break;

  case 87:
#line 395 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), declFieldInit(""));
        }
    break;

  case 88:
#line 401 "parser.yy"
    { types = 0; }
    break;

  case 91:
#line 405 "parser.yy"
    { CALL((yylsp[(1) - (4)]), (yylsp[(3) - (4)]), typeArrayOfSize(types)); }
    break;

  case 92:
#line 406 "parser.yy"
    { types++; }
    break;

  case 93:
#line 406 "parser.yy"
    { CALL((yylsp[(1) - (5)]), (yylsp[(3) - (5)]), typeArrayOfType(types--)); }
    break;

  case 95:
#line 411 "parser.yy"
    {
          CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), typePop());
        }
    break;

  case 99:
#line 423 "parser.yy"
    {
            CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeDuplicate());
        }
    break;

  case 100:
#line 425 "parser.yy"
    { 
            CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), declTypeDef((yyvsp[(1) - (3)].string)));
        }
    break;

  case 101:
#line 431 "parser.yy"
    { 
            CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeName(ParserBuilder::PREFIX_NONE, (yyvsp[(1) - (1)].string)));
        }
    break;

  case 102:
#line 434 "parser.yy"
    { 
            CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), typeName((yyvsp[(1) - (2)].prefix), (yyvsp[(2) - (2)].string)));
        }
    break;

  case 103:
#line 437 "parser.yy"
    { 
            CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), typeStruct(ParserBuilder::PREFIX_NONE, (yyvsp[(3) - (4)].number)));
        }
    break;

  case 104:
#line 440 "parser.yy"
    { 
            CALL((yylsp[(1) - (5)]), (yylsp[(5) - (5)]), typeStruct((yyvsp[(1) - (5)].prefix), (yyvsp[(4) - (5)].number)));
        }
    break;

  case 105:
#line 443 "parser.yy"
    { 
          CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), typeStruct(ParserBuilder::PREFIX_NONE, 0));
        }
    break;

  case 106:
#line 446 "parser.yy"
    { 
          CALL((yylsp[(1) - (5)]), (yylsp[(5) - (5)]), typeStruct(ParserBuilder::PREFIX_NONE, 0));
        }
    break;

  case 107:
#line 449 "parser.yy"
    {
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeBool(ParserBuilder::PREFIX_NONE));
        }
    break;

  case 108:
#line 452 "parser.yy"
    {
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), typeBool((yyvsp[(1) - (2)].prefix)));
        }
    break;

  case 109:
#line 455 "parser.yy"
    {
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeInt(ParserBuilder::PREFIX_NONE));
        }
    break;

  case 110:
#line 458 "parser.yy"
    {
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), typeInt((yyvsp[(1) - (2)].prefix)));
        }
    break;

  case 111:
#line 462 "parser.yy"
    {
          CALL((yylsp[(1) - (6)]), (yylsp[(6) - (6)]), typeBoundedInt(ParserBuilder::PREFIX_NONE));
        }
    break;

  case 112:
#line 465 "parser.yy"
    {
          CALL((yylsp[(1) - (7)]), (yylsp[(7) - (7)]), typeBoundedInt((yyvsp[(1) - (7)].prefix)));
        }
    break;

  case 113:
#line 468 "parser.yy"
    {
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeChannel(ParserBuilder::PREFIX_NONE));
        }
    break;

  case 114:
#line 471 "parser.yy"
    {
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), typeChannel((yyvsp[(1) - (2)].prefix)));
        }
    break;

  case 115:
#line 474 "parser.yy"
    {
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeClock());
        }
    break;

  case 116:
#line 477 "parser.yy"
    {
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeVoid());
        }
    break;

  case 117:
#line 481 "parser.yy"
    {
          CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), typeScalar(ParserBuilder::PREFIX_NONE));
        }
    break;

  case 118:
#line 484 "parser.yy"
    {
          CALL((yylsp[(1) - (5)]), (yylsp[(5) - (5)]), typeScalar((yyvsp[(1) - (5)].prefix)));
        }
    break;

  case 119:
#line 490 "parser.yy"
    { strncpy((yyval.string), (yyvsp[(1) - (1)].string), MAXLEN); }
    break;

  case 120:
#line 491 "parser.yy"
    { strncpy((yyval.string), (yyvsp[(1) - (1)].string), MAXLEN); }
    break;

  case 121:
#line 495 "parser.yy"
    { strncpy((yyval.string), (yyvsp[(1) - (1)].string) , MAXLEN); }
    break;

  case 122:
#line 496 "parser.yy"
    { strncpy((yyval.string), "A", MAXLEN); }
    break;

  case 123:
#line 497 "parser.yy"
    { strncpy((yyval.string), "U", MAXLEN); }
    break;

  case 124:
#line 498 "parser.yy"
    { strncpy((yyval.string), "W", MAXLEN); }
    break;

  case 125:
#line 499 "parser.yy"
    { strncpy((yyval.string), "inf", MAXLEN); }
    break;

  case 126:
#line 500 "parser.yy"
    { strncpy((yyval.string), "sup", MAXLEN); }
    break;

  case 127:
#line 504 "parser.yy"
    { (yyval.number)=(yyvsp[(1) - (1)].number); }
    break;

  case 128:
#line 505 "parser.yy"
    { (yyval.number)=(yyvsp[(1) - (2)].number)+(yyvsp[(2) - (2)].number); }
    break;

  case 129:
#line 509 "parser.yy"
    {
          (yyval.number) = (yyvsp[(2) - (3)].number); 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), typePop());
        }
    break;

  case 130:
#line 516 "parser.yy"
    { (yyval.number)=1; }
    break;

  case 131:
#line 517 "parser.yy"
    { (yyval.number)=(yyvsp[(1) - (3)].number)+1; }
    break;

  case 132:
#line 521 "parser.yy"
    {
            CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeDuplicate());
        }
    break;

  case 133:
#line 523 "parser.yy"
    { 
            CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), structField((yyvsp[(1) - (3)].string)));
        }
    break;

  case 134:
#line 529 "parser.yy"
    { (yyval.prefix) = ParserBuilder::PREFIX_URGENT; }
    break;

  case 135:
#line 530 "parser.yy"
    { (yyval.prefix) = ParserBuilder::PREFIX_BROADCAST; }
    break;

  case 136:
#line 531 "parser.yy"
    { (yyval.prefix) = ParserBuilder::PREFIX_URGENT_BROADCAST; }
    break;

  case 137:
#line 532 "parser.yy"
    { (yyval.prefix) = ParserBuilder::PREFIX_CONST; }
    break;

  case 138:
#line 533 "parser.yy"
    { (yyval.prefix) = ParserBuilder::PREFIX_META; }
    break;

  case 139:
#line 541 "parser.yy"
    { 
          CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), procBegin((yyvsp[(2) - (4)].string)));
        }
    break;

  case 140:
#line 544 "parser.yy"
    { 
          CALL((yylsp[(6) - (7)]), (yylsp[(7) - (7)]), procEnd());
        }
    break;

  case 150:
#line 571 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), procState((yyvsp[(1) - (1)].string), false));
        }
    break;

  case 151:
#line 574 "parser.yy"
    { 
          CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), procState((yyvsp[(1) - (4)].string), true));
        }
    break;

  case 152:
#line 577 "parser.yy"
    { 
          CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), procState((yyvsp[(1) - (4)].string), false));
        }
    break;

  case 153:
#line 583 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), procStateInit((yyvsp[(2) - (3)].string)));
        }
    break;

  case 160:
#line 601 "parser.yy"
    {
            CALL((yylsp[(1) - (4)]), (yylsp[(3) - (4)]), procEdgeBegin((yyvsp[(1) - (4)].string), (yyvsp[(3) - (4)].string), true));
        }
    break;

  case 161:
#line 603 "parser.yy"
    { 
          strcpy(rootTransId, (yyvsp[(1) - (10)].string)); 
          CALL((yylsp[(1) - (10)]), (yylsp[(8) - (10)]), procEdgeEnd((yyvsp[(1) - (10)].string), (yyvsp[(3) - (10)].string)));
        }
    break;

  case 162:
#line 607 "parser.yy"
    {
            CALL((yylsp[(1) - (4)]), (yylsp[(3) - (4)]), procEdgeBegin((yyvsp[(1) - (4)].string), (yyvsp[(3) - (4)].string), false));
        }
    break;

  case 163:
#line 609 "parser.yy"
    { 
          strcpy(rootTransId, (yyvsp[(1) - (10)].string)); 
          CALL((yylsp[(1) - (10)]), (yylsp[(8) - (10)]), procEdgeEnd((yyvsp[(1) - (10)].string), (yyvsp[(3) - (10)].string)));
        }
    break;

  case 164:
#line 616 "parser.yy"
    { 
            CALL((yylsp[(1) - (3)]), (yylsp[(2) - (3)]), procEdgeBegin(rootTransId, (yyvsp[(2) - (3)].string), true)); 
        }
    break;

  case 165:
#line 618 "parser.yy"
    { 
            CALL((yylsp[(1) - (9)]), (yylsp[(7) - (9)]), procEdgeEnd(rootTransId, (yyvsp[(2) - (9)].string)));
        }
    break;

  case 166:
#line 621 "parser.yy"
    { 
            CALL((yylsp[(1) - (3)]), (yylsp[(2) - (3)]), procEdgeBegin(rootTransId, (yyvsp[(2) - (3)].string), false)); 
        }
    break;

  case 167:
#line 623 "parser.yy"
    { 
            CALL((yylsp[(1) - (9)]), (yylsp[(7) - (9)]), procEdgeEnd(rootTransId, (yyvsp[(2) - (9)].string)));
        }
    break;

  case 171:
#line 635 "parser.yy"
    {
            CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), procSelect((yyvsp[(1) - (3)].string)));
        }
    break;

  case 172:
#line 638 "parser.yy"
    {
            CALL((yylsp[(3) - (5)]), (yylsp[(5) - (5)]), procSelect((yyvsp[(3) - (5)].string)));
        }
    break;

  case 174:
#line 644 "parser.yy"
    {
          CALL((yylsp[(2) - (3)]), (yylsp[(2) - (3)]), procGuard());
        }
    break;

  case 175:
#line 647 "parser.yy"
    {
          CALL((yylsp[(2) - (4)]), (yylsp[(3) - (4)]), procGuard());
        }
    break;

  case 180:
#line 660 "parser.yy"
    { 
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), procSync(SYNC_BANG));
        }
    break;

  case 181:
#line 663 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(2) - (3)]), procSync(SYNC_QUE));
        }
    break;

  case 182:
#line 666 "parser.yy"
    { 
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), procSync(SYNC_QUE));
        }
    break;

  case 183:
#line 669 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(2) - (3)]), procSync(SYNC_QUE));
        }
    break;

  case 185:
#line 676 "parser.yy"
    {
          CALL((yylsp[(2) - (3)]), (yylsp[(2) - (3)]), procUpdate());	  
        }
    break;

  case 194:
#line 699 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), procStateCommit((yyvsp[(1) - (1)].string)));
        }
    break;

  case 195:
#line 702 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), procStateCommit((yyvsp[(3) - (3)].string)));
        }
    break;

  case 196:
#line 708 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), procStateUrgent((yyvsp[(1) - (1)].string)));
        }
    break;

  case 197:
#line 711 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), procStateUrgent((yyvsp[(3) - (3)].string)));
        }
    break;

  case 198:
#line 721 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), blockBegin());
        }
    break;

  case 199:
#line 724 "parser.yy"
    { 
          CALL((yylsp[(2) - (5)]), (yylsp[(4) - (5)]), blockEnd());
        }
    break;

  case 207:
#line 743 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), emptyStatement());
        }
    break;

  case 208:
#line 746 "parser.yy"
    { 
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprStatement());
        }
    break;

  case 209:
#line 749 "parser.yy"
    { 
          CALL((yylsp[(1) - (8)]), (yylsp[(8) - (8)]), forBegin());
        }
    break;

  case 210:
#line 752 "parser.yy"
    { 
            CALL((yylsp[(9) - (10)]), (yylsp[(9) - (10)]), forEnd());
        }
    break;

  case 211:
#line 755 "parser.yy"
    { 
            CALL((yylsp[(1) - (6)]), (yylsp[(6) - (6)]), iterationBegin((yyvsp[(3) - (6)].string)));
        }
    break;

  case 212:
#line 758 "parser.yy"
    { 
            CALL((yylsp[(7) - (8)]), (yylsp[(7) - (8)]), iterationEnd((yyvsp[(3) - (8)].string)));
        }
    break;

  case 214:
#line 762 "parser.yy"
    {
            CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), whileBegin());
        }
    break;

  case 215:
#line 765 "parser.yy"
    { 
            CALL((yylsp[(3) - (6)]), (yylsp[(4) - (6)]), whileEnd());
          }
    break;

  case 217:
#line 769 "parser.yy"
    { 
            CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), doWhileBegin());
        }
    break;

  case 218:
#line 772 "parser.yy"
    { 
            CALL((yylsp[(2) - (8)]), (yylsp[(7) - (8)]), doWhileEnd());
          }
    break;

  case 219:
#line 775 "parser.yy"
    { 
            CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), ifBegin());
        }
    break;

  case 221:
#line 779 "parser.yy"
    { 
            CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), breakStatement());
          }
    break;

  case 222:
#line 782 "parser.yy"
    { 
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), continueStatement());
        }
    break;

  case 223:
#line 785 "parser.yy"
    { 
            CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), switchBegin());
        }
    break;

  case 224:
#line 788 "parser.yy"
    { 
               CALL((yylsp[(5) - (8)]), (yylsp[(7) - (8)]), switchEnd());
           }
    break;

  case 225:
#line 791 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), returnStatement(true));
        }
    break;

  case 226:
#line 794 "parser.yy"
    { 
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), returnStatement(false));
        }
    break;

  case 227:
#line 797 "parser.yy"
    {
	    CALL((yylsp[(1) - (3)]), (yylsp[(2) - (3)]), assertStatement());
	}
    break;

  case 228:
#line 803 "parser.yy"
    { 
          CALL(position_t(), position_t(), ifEnd(false));
        }
    break;

  case 229:
#line 806 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), ifElse());
        }
    break;

  case 230:
#line 809 "parser.yy"
    { 
          CALL((yylsp[(2) - (3)]), (yylsp[(2) - (3)]), ifEnd(true));
          }
    break;

  case 233:
#line 820 "parser.yy"
    { 
	    CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), caseBegin());
        }
    break;

  case 234:
#line 823 "parser.yy"
    { 
            CALL((yylsp[(4) - (5)]), (yylsp[(4) - (5)]), caseEnd());
	}
    break;

  case 235:
#line 826 "parser.yy"
    { 
	    CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), caseBegin());
        }
    break;

  case 236:
#line 829 "parser.yy"
    {
            CALL((yylsp[(4) - (5)]), (yylsp[(4) - (5)]), caseEnd());
	}
    break;

  case 237:
#line 832 "parser.yy"
    { 
            CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), defaultBegin());
        }
    break;

  case 238:
#line 835 "parser.yy"
    { 
            CALL((yylsp[(3) - (4)]), (yylsp[(3) - (4)]), defaultEnd());
        }
    break;

  case 239:
#line 838 "parser.yy"
    { 
	    CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), defaultBegin());
        }
    break;

  case 240:
#line 841 "parser.yy"
    { 
            CALL((yylsp[(4) - (5)]), (yylsp[(4) - (5)]), defaultEnd());
        }
    break;

  case 242:
#line 848 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprComma());
        }
    break;

  case 243:
#line 853 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), exprFalse());
        }
    break;

  case 244:
#line 856 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), exprTrue());
        }
    break;

  case 245:
#line 859 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), exprNat((yyvsp[(1) - (1)].number)));
        }
    break;

  case 246:
#line 862 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), exprId((yyvsp[(1) - (1)].string)));
        }
    break;

  case 247:
#line 865 "parser.yy"
    {
            CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprCallBegin());	    
          }
    break;

  case 248:
#line 867 "parser.yy"
    { 
            CALL((yylsp[(1) - (5)]), (yylsp[(5) - (5)]), exprCallEnd((yyvsp[(4) - (5)].number)));
        }
    break;

  case 249:
#line 870 "parser.yy"
    {
            CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprCallBegin());
          }
    break;

  case 250:
#line 872 "parser.yy"
    {   
            CALL((yylsp[(1) - (5)]), (yylsp[(5) - (5)]), exprCallEnd(0));
        }
    break;

  case 251:
#line 875 "parser.yy"
    { 
          CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), exprArray());
        }
    break;

  case 252:
#line 878 "parser.yy"
    { 
          CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), exprFalse());
        }
    break;

  case 254:
#line 882 "parser.yy"
    {
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprFalse());
        }
    break;

  case 255:
#line 885 "parser.yy"
    { 
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprPostIncrement());
        }
    break;

  case 256:
#line 888 "parser.yy"
    { 
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprPreIncrement());
        }
    break;

  case 257:
#line 891 "parser.yy"
    { 
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprPostDecrement());
        }
    break;

  case 258:
#line 894 "parser.yy"
    { 
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprPreDecrement());
        }
    break;

  case 259:
#line 897 "parser.yy"
    { 
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprUnary((yyvsp[(1) - (2)].kind)));
        }
    break;

  case 260:
#line 900 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(LT));
        }
    break;

  case 261:
#line 903 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(LE));
        }
    break;

  case 262:
#line 906 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(EQ));
        }
    break;

  case 263:
#line 909 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(NEQ));
        }
    break;

  case 264:
#line 912 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(GT));
        }
    break;

  case 265:
#line 915 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(GE));
        }
    break;

  case 266:
#line 918 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(PLUS));
        }
    break;

  case 267:
#line 921 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(MINUS));
        }
    break;

  case 268:
#line 924 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(MULT));
        }
    break;

  case 269:
#line 927 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(DIV));
        }
    break;

  case 270:
#line 930 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(MOD));
        }
    break;

  case 271:
#line 933 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(BIT_AND));
        }
    break;

  case 272:
#line 936 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(BIT_OR));
        }
    break;

  case 273:
#line 939 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(BIT_XOR));
        }
    break;

  case 274:
#line 942 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(BIT_LSHIFT));
        }
    break;

  case 275:
#line 945 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(BIT_RSHIFT));
        }
    break;

  case 276:
#line 948 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(AND));
        }
    break;

  case 277:
#line 951 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(OR));
        }
    break;

  case 278:
#line 954 "parser.yy"
    { 
          CALL((yylsp[(1) - (5)]), (yylsp[(5) - (5)]), exprInlineIf());
        }
    break;

  case 279:
#line 957 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprDot((yyvsp[(3) - (3)].string)));
        }
    break;

  case 280:
#line 960 "parser.yy"
    {
            CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprUnary(RATE));
        }
    break;

  case 281:
#line 963 "parser.yy"
    {
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), exprDeadlock());
        }
    break;

  case 282:
#line 966 "parser.yy"
    {  
          CALL((yylsp[(1) - (2)]), (yylsp[(1) - (2)]), exprUnary(NOT));
        }
    break;

  case 283:
#line 968 "parser.yy"
    {
          CALL((yylsp[(3) - (4)]), (yylsp[(3) - (4)]), exprBinary(OR));
        }
    break;

  case 284:
#line 971 "parser.yy"
    {
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(AND));
        }
    break;

  case 285:
#line 974 "parser.yy"
    {
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(OR));
        }
    break;

  case 286:
#line 977 "parser.yy"
    {
          CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprUnary(NOT));
        }
    break;

  case 287:
#line 980 "parser.yy"
    {
            CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(MIN));
        }
    break;

  case 288:
#line 983 "parser.yy"
    {
            CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(MAX));
        }
    break;

  case 289:
#line 986 "parser.yy"
    {
            CALL((yylsp[(1) - (6)]), (yylsp[(6) - (6)]), exprForAllBegin((yyvsp[(3) - (6)].string)));
        }
    break;

  case 290:
#line 988 "parser.yy"
    {
            CALL((yylsp[(1) - (8)]), (yylsp[(8) - (8)]), exprForAllEnd((yyvsp[(3) - (8)].string)));
        }
    break;

  case 291:
#line 991 "parser.yy"
    {
            CALL((yylsp[(1) - (6)]), (yylsp[(6) - (6)]), exprExistsBegin((yyvsp[(3) - (6)].string)));
        }
    break;

  case 292:
#line 993 "parser.yy"
    {
            CALL((yylsp[(1) - (8)]), (yylsp[(8) - (8)]), exprExistsEnd((yyvsp[(3) - (8)].string)));
        }
    break;

  case 294:
#line 997 "parser.yy"
    {
	    CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprUnary(AF));
	}
    break;

  case 295:
#line 1000 "parser.yy"
    {
	    CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprUnary(AG));
        }
    break;

  case 296:
#line 1003 "parser.yy"
    {
	    CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprUnary(EF));
        }
    break;

  case 297:
#line 1006 "parser.yy"
    {
	    CALL((yylsp[(1) - (3)]), (yylsp[(2) - (3)]), exprUnary(AG_R));
        }
    break;

  case 298:
#line 1009 "parser.yy"
    {
	    CALL((yylsp[(1) - (3)]), (yylsp[(2) - (3)]), exprUnary(EF_R));
        }
    break;

  case 299:
#line 1012 "parser.yy"
    {
	    CALL((yylsp[(1) - (2)]), (yylsp[(2) - (2)]), exprUnary(EG));
        }
    break;

  case 300:
#line 1015 "parser.yy"
    {
	    CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(AF2));
	}
    break;

  case 301:
#line 1018 "parser.yy"
    {
	    CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(AG2));
        }
    break;

  case 302:
#line 1021 "parser.yy"
    {
	    CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(EF2));
	}
    break;

  case 303:
#line 1024 "parser.yy"
    {
	    CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(EG2));
	}
    break;

  case 304:
#line 1027 "parser.yy"
    {
	    CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(LEADSTO));
        }
    break;

  case 305:
#line 1030 "parser.yy"
    {
	    CALL((yylsp[(1) - (6)]), (yylsp[(6) - (6)]), exprTernary(A_UNTIL, true));
        }
    break;

  case 306:
#line 1033 "parser.yy"
    {
	    CALL((yylsp[(1) - (6)]), (yylsp[(6) - (6)]), exprTernary(A_WEAKUNTIL, true));
        }
    break;

  case 307:
#line 1036 "parser.yy"
    {
	    CALL((yylsp[(1) - (9)]), (yylsp[(7) - (9)]), exprTernary(A_UNTIL));
        }
    break;

  case 308:
#line 1039 "parser.yy"
    {
	    CALL((yylsp[(1) - (9)]), (yylsp[(7) - (9)]), exprTernary(A_WEAKUNTIL));
        }
    break;

  case 309:
#line 1045 "parser.yy"
    { ch->exprTrue(); }
    break;

  case 311:
#line 1049 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprAssignment((yyvsp[(2) - (3)].kind)));
        }
    break;

  case 312:
#line 1055 "parser.yy"
    { (yyval.kind) = ASSIGN; }
    break;

  case 313:
#line 1056 "parser.yy"
    { (yyval.kind) = ASSPLUS; }
    break;

  case 314:
#line 1057 "parser.yy"
    { (yyval.kind) = ASSMINUS; }
    break;

  case 315:
#line 1058 "parser.yy"
    { (yyval.kind) = ASSDIV; }
    break;

  case 316:
#line 1059 "parser.yy"
    { (yyval.kind) = ASSMOD; }
    break;

  case 317:
#line 1060 "parser.yy"
    { (yyval.kind) = ASSMULT; }
    break;

  case 318:
#line 1061 "parser.yy"
    { (yyval.kind) = ASSAND; }
    break;

  case 319:
#line 1062 "parser.yy"
    { (yyval.kind) = ASSOR; }
    break;

  case 320:
#line 1063 "parser.yy"
    { (yyval.kind) = ASSXOR; }
    break;

  case 321:
#line 1064 "parser.yy"
    { (yyval.kind) = ASSLSHIFT; }
    break;

  case 322:
#line 1065 "parser.yy"
    { (yyval.kind) = ASSRSHIFT; }
    break;

  case 323:
#line 1070 "parser.yy"
    { (yyval.kind) = MINUS; }
    break;

  case 324:
#line 1071 "parser.yy"
    { (yyval.kind) = PLUS; }
    break;

  case 325:
#line 1072 "parser.yy"
    { (yyval.kind) = NOT; }
    break;

  case 326:
#line 1077 "parser.yy"
    { (yyval.number)=0; }
    break;

  case 327:
#line 1078 "parser.yy"
    { 
            (yyval.number) = 1; 
        }
    break;

  case 328:
#line 1081 "parser.yy"
    { 
            (yyval.number) = (yyvsp[(1) - (3)].number) + 1; 
        }
    break;

  case 334:
#line 1102 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeInt(ParserBuilder::PREFIX_CONST));
        }
    break;

  case 335:
#line 1104 "parser.yy"
    { 
          CALL((yylsp[(1) - (4)]), (yylsp[(3) - (4)]), typePop());
        }
    break;

  case 339:
#line 1115 "parser.yy"
    {
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeDuplicate());
        }
    break;

  case 340:
#line 1117 "parser.yy"
    { 
          CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), declVar((yyvsp[(1) - (4)].string), true));
        }
    break;

  case 341:
#line 1126 "parser.yy"
    { 
          CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), procBegin((yyvsp[(2) - (4)].string)));
        }
    break;

  case 342:
#line 1129 "parser.yy"
    { 
          CALL((yylsp[(5) - (7)]), (yylsp[(6) - (7)]), procEnd());
        }
    break;

  case 343:
#line 1132 "parser.yy"
    { 
          CALL((yylsp[(1) - (5)]), (yylsp[(5) - (5)]), procBegin((yyvsp[(2) - (5)].string)));
        }
    break;

  case 344:
#line 1135 "parser.yy"
    { 
          CALL((yylsp[(6) - (8)]), (yylsp[(7) - (8)]), procEnd());
        }
    break;

  case 345:
#line 1138 "parser.yy"
    { 
          CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), procBegin((yyvsp[(2) - (4)].string)));
        }
    break;

  case 346:
#line 1141 "parser.yy"
    { 
          CALL((yylsp[(5) - (7)]), (yylsp[(6) - (7)]), procEnd());
        }
    break;

  case 347:
#line 1144 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), procBegin("_"));
        }
    break;

  case 348:
#line 1147 "parser.yy"
    { 
          CALL((yylsp[(4) - (6)]), (yylsp[(5) - (6)]), procEnd());
        }
    break;

  case 349:
#line 1150 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), procBegin((yyvsp[(2) - (3)].string)));
        }
    break;

  case 350:
#line 1153 "parser.yy"
    { 
          CALL((yylsp[(4) - (6)]), (yylsp[(5) - (6)]), procEnd());
        }
    break;

  case 354:
#line 1165 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typePop());
        }
    break;

  case 356:
#line 1169 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), typePop());
        }
    break;

  case 358:
#line 1176 "parser.yy"
    {
            CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeDuplicate());
        }
    break;

  case 359:
#line 1178 "parser.yy"
    {
            CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), declParameter((yyvsp[(3) - (4)].string), true));
        }
    break;

  case 360:
#line 1181 "parser.yy"
    {
            CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeDuplicate());
        }
    break;

  case 361:
#line 1183 "parser.yy"
    { 
            CALL((yylsp[(1) - (5)]), (yylsp[(5) - (5)]), declParameter((yyvsp[(4) - (5)].string), true));
        }
    break;

  case 362:
#line 1189 "parser.yy"
    {
            CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), typeInt(ParserBuilder::PREFIX_CONST));
        }
    break;

  case 363:
#line 1191 "parser.yy"
    {
            CALL((yylsp[(3) - (4)]), (yylsp[(4) - (4)]), declParameter((yyvsp[(3) - (4)].string), false));
        }
    break;

  case 364:
#line 1194 "parser.yy"
    {
            CALL((yylsp[(1) - (2)]), (yylsp[(1) - (2)]), typeInt(ParserBuilder::PREFIX_CONST));
        }
    break;

  case 365:
#line 1196 "parser.yy"
    { 
            CALL((yylsp[(4) - (5)]), (yylsp[(5) - (5)]), declParameter((yyvsp[(4) - (5)].string), false));
        }
    break;

  case 373:
#line 1221 "parser.yy"
    { 
          CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), exprTrue(); ch->procState((yyvsp[(1) - (1)].string), false));
        }
    break;

  case 374:
#line 1224 "parser.yy"
    { 
          CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), procState((yyvsp[(1) - (4)].string), true));
        }
    break;

  case 376:
#line 1231 "parser.yy"
    {	  
        }
    break;

  case 377:
#line 1233 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(AND));
        }
    break;

  case 383:
#line 1250 "parser.yy"
    {
            CALL((yylsp[(1) - (4)]), (yylsp[(3) - (4)]), procEdgeBegin((yyvsp[(1) - (4)].string), (yyvsp[(3) - (4)].string), true));
        }
    break;

  case 384:
#line 1252 "parser.yy"
    { 
            strcpy(rootTransId, (yyvsp[(1) - (9)].string));
            CALL((yylsp[(1) - (9)]), (yylsp[(8) - (9)]), procEdgeEnd((yyvsp[(1) - (9)].string), (yyvsp[(3) - (9)].string)));
        }
    break;

  case 385:
#line 1260 "parser.yy"
    {
            CALL((yylsp[(1) - (3)]), (yylsp[(2) - (3)]), procEdgeBegin(rootTransId, (yyvsp[(2) - (3)].string), true));
        }
    break;

  case 386:
#line 1262 "parser.yy"
    { 
            CALL((yylsp[(1) - (8)]), (yylsp[(7) - (8)]), procEdgeEnd(rootTransId, (yyvsp[(2) - (8)].string)));
        }
    break;

  case 389:
#line 1270 "parser.yy"
    {
          CALL((yylsp[(2) - (3)]), (yylsp[(2) - (3)]), procGuard());
        }
    break;

  case 390:
#line 1273 "parser.yy"
    {
          CALL((yylsp[(2) - (4)]), (yylsp[(3) - (4)]), procGuard());
        }
    break;

  case 392:
#line 1280 "parser.yy"
    { 
          CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprBinary(AND));
        }
    break;

  case 396:
#line 1293 "parser.yy"
    {
	    CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), declParamId((yyvsp[(1) - (1)].string)));
        }
    break;

  case 397:
#line 1298 "parser.yy"
    {
	    (yyval.number) = 1;
	}
    break;

  case 398:
#line 1301 "parser.yy"
    {
	    (yyval.number) = (yyvsp[(1) - (3)].number) + 1;
	}
    break;

  case 399:
#line 1306 "parser.yy"
    {
            CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), property());
	}
    break;

  case 400:
#line 1309 "parser.yy"
    {
            CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), exprUnary(CONTROL));
            CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), property());
        }
    break;

  case 401:
#line 1313 "parser.yy"
    {
	    CALL((yylsp[(1) - (9)]), (yylsp[(9) - (9)]), exprTernary(CONTROL_TOPT));
	    CALL((yylsp[(1) - (9)]), (yylsp[(9) - (9)]), property());
	}
    break;

  case 402:
#line 1317 "parser.yy"
    {
	    CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), exprUnary(EF_CONTROL));
	    CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), property());
	}
    break;

  case 405:
#line 1325 "parser.yy"
    {
	    CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), paramProperty((yyvsp[(2) - (4)].number), INF));
        }
    break;

  case 406:
#line 1328 "parser.yy"
    {
	    CALL((yylsp[(1) - (4)]), (yylsp[(4) - (4)]), paramProperty((yyvsp[(2) - (4)].number), SUP));
        }
    break;

  case 408:
#line 1334 "parser.yy"
    {
	  CALL((yylsp[(1) - (1)]), (yylsp[(1) - (1)]), ssQueryBegin());
	}
    break;

  case 409:
#line 1336 "parser.yy"
    {
	  CALL((yylsp[(1) - (8)]), (yylsp[(2) - (8)]), ssQueryEnd());
	}
    break;


/* Line 1267 of yacc.c.  */
#line 5463 "parser.tab.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
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
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }

  yyerror_range[0] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval, &yylloc);
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

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  yyerror_range[0] = yylsp[1-yylen];
  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
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
      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, yylsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;

  yyerror_range[1] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the look-ahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, (yyerror_range - 1), 2);
  *++yylsp = yyloc;

  /* Shift the error token.  */
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
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, &yylloc);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, yylsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 1340 "parser.yy"


#include "lexer.cc"

static void utap_error(const char* msg)
{
    ch->setPosition(yylloc.start, yylloc.end);
    ch->handleError(msg);
}

static void setStartToken(xta_part_t part, bool newxta)
{
    switch (part) 
    {
    case S_XTA: 
        syntax_token = newxta ? T_NEW : T_OLD;
        break;
    case S_DECLARATION: 
        syntax_token = newxta ? T_NEW_DECLARATION : T_OLD_DECLARATION;
        break;
    case S_LOCAL_DECL: 
        syntax_token = newxta ? T_NEW_LOCAL_DECL : T_OLD_LOCAL_DECL; 
        break;
    case S_INST: 
        syntax_token = newxta ? T_NEW_INST : T_OLD_INST;
        break;
    case S_SYSTEM: 
        syntax_token = T_NEW_SYSTEM;
        break;
    case S_PARAMETERS: 
        syntax_token = newxta ? T_NEW_PARAMETERS : T_OLD_PARAMETERS;
        break;
    case S_INVARIANT: 
        syntax_token = newxta ? T_NEW_INVARIANT : T_OLD_INVARIANT;
        break;
    case S_SELECT:
        syntax_token = T_NEW_SELECT;
        break;
    case S_GUARD: 
        syntax_token = newxta ? T_NEW_GUARD : T_OLD_GUARD; 
        break;
    case S_SYNC: 
        syntax_token = T_NEW_SYNC;
        break;
    case S_ASSIGN: 
        syntax_token = newxta ? T_NEW_ASSIGN : T_OLD_ASSIGN;
        break;
    case S_EXPRESSION:
        syntax_token = T_EXPRESSION;
        break;
    case S_PROPERTY:
        syntax_token = T_PROPERTY;
        break;
    }
}

static int32_t parseXTA(ParserBuilder *aParserBuilder,
        		bool newxta, xta_part_t part, std::string xpath)
{
    // Select syntax
    syntax = (syntax_t)((newxta ? SYNTAX_NEW : SYNTAX_OLD) | SYNTAX_GUIDING);
    setStartToken(part, newxta);

    // Set parser builder 
    ch = aParserBuilder;
    
    // Reset position tracking
    PositionTracker::setPath(ch, xpath);

    // Parse string
    int res = 0;

    if (utap_parse()) 
    {
        res = -1;
    }

    ch = NULL;
    return res;
}

static int32_t parseProperty(ParserBuilder *aParserBuilder, bool ssql)
{
    // Select syntax
    syntax = (syntax_t)(ssql ? (SYNTAX_PROPERTY | SYNTAX_SSQL) : SYNTAX_PROPERTY);
    setStartToken(S_PROPERTY, false);

    // Set parser builder 
    ch = aParserBuilder;
    
    // Reset position tracking
    PositionTracker::setPath(ch, "");

    return utap_parse() ? -1 : 0;
}

int32_t parseXTA(const char *str, ParserBuilder *builder,
        	 bool newxta, xta_part_t part, std::string xpath)
{
    utap__scan_string(str);
    int32_t res = parseXTA(builder, newxta, part, xpath);
    utap__delete_buffer(YY_CURRENT_BUFFER);
    return res;
}

int32_t parseXTA(const char *str, ParserBuilder *builder, bool newxta)
{
    return parseXTA(str, builder, newxta, S_XTA, "");
}

int32_t parseXTA(FILE *file, ParserBuilder *builder, bool newxta)
{
    utap__switch_to_buffer(utap__create_buffer(file, YY_BUF_SIZE));
    int res = parseXTA(builder, newxta, S_XTA, "");
    utap__delete_buffer(YY_CURRENT_BUFFER);
    return res;
}

int32_t parseProperty(const char *str, ParserBuilder *aParserBuilder, bool ssql)
{
    utap__scan_string(str);
    int32_t res = parseProperty(aParserBuilder, ssql);
    utap__delete_buffer(YY_CURRENT_BUFFER);
    return res;
}

int32_t parseProperty(FILE *file, ParserBuilder *aParserBuilder, bool ssql)
{
    utap__switch_to_buffer(utap__create_buffer(file, YY_BUF_SIZE));
    int32_t res = parseProperty(aParserBuilder, ssql);
    utap__delete_buffer(YY_CURRENT_BUFFER);
    return res;
}


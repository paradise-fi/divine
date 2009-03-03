
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
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
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 1

/* Substitute the variable and function names.  */
#define yyparse         dve_ppparse
#define yylex           dve_pplex
#define yyerror         dve_pperror
#define yylval dve_yylval
#define yychar          dve_ppchar
#define yydebug         dve_ppdebug
#define yynerrs         dve_ppnerrs
#define yylloc dve_yylloc

/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */


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


/* Line 189 of yacc.c  */


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

/* Line 214 of yacc.c  */


  bool flag;
  int number;
  char string[MAXLEN];



/* Line 214 of yacc.c  */

} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
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


/* Line 264 of yacc.c  */


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
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
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
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
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
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  6
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   541

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  75
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  70
/* YYNRULES -- Number of rules.  */
#define YYNRULES  153
/* YYNRULES -- Number of states.  */
#define YYNSTATES  268

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   318

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
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
static const yytype_uint16 yyprhs[] =
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

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
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
static const yytype_uint16 yyrline[] =
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

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
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
  "'}'", "':'", "'?'", "$accept", "Id", "VariableDecl", "$@1", "$@2",
  "Const", "TypeName", "TypeId", "DeclIdList", "DeclId", "VarInit", "$@3",
  "Initializer", "FieldInitList", "FieldInit", "ArrayDecl",
  "ProcDeclStandalone", "ProcDecl", "$@4", "ProcBody", "ProcLocalDeclList",
  "States", "StateDeclList", "StateDecl", "Init", "InitAndCommitAndAccept",
  "CommitAndAccept", "Accept", "AcceptListBuchi",
  "AcceptListGenBuchiMuller", "AcceptListGenBuchiMullerSet",
  "AcceptListGenBuchiMullerSetNotEmpty", "AcceptListRabinStreett",
  "AcceptListRabinStreettPair", "AcceptListRabinStreettFirst",
  "AcceptListRabinStreettSecond", "AcceptListRabinStreettSet", "Commit",
  "CommitList", "Assertions", "AssertionList", "Assertion", "$@5",
  "Transitions", "TransitionList", "Transition", "ProbTransition",
  "ProbTransitionPart", "ProbList", "TransitionOpt", "Guard", "$@6", "$@7",
  "Sync", "SyncExpr", "$@8", "$@9", "$@10", "$@11", "SyncValue",
  "ExpressionList", "OneExpression", "Effect", "$@12", "$@13", "EffList",
  "Expression", "Assignment", "AssignOp", "UnaryOp", 0
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
      40,    41,    91,    93,    46,   315,   316,   317,   318,    59,
      44,   123,   125,    58,    63
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
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
static const yytype_uint8 yyr2[] =
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
static const yytype_uint8 yydefact[] =
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

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
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
static const yytype_int16 yypact[] =
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
static const yytype_int16 yypgoto[] =
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
static const yytype_int16 yytable[] =
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

static const yytype_int16 yycheck[] =
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
static const yytype_uint8 yystos[] =
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
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
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
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       , &(yylsp[(yyi + 1) - (yynrhs)])		       );
      YYFPRINTF (stderr, "\n");
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


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
extern YYSTYPE dve_yylval;

/* Location data for the lookahead symbol.  */
extern YYLTYPE dve_yylloc;

/* Number of syntax errors so far.  */
int yynerrs;



/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

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
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.
       `yyls': related to locations.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    /* The location stack.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;

    /* The locations where the error started and ended.  */
    YYLTYPE yyerror_range[2];

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yyls = yylsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

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
  yylloc.first_column = yylloc.last_column = 1;
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
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
	YYSTACK_RELOCATE (yyls_alloc, yyls);
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

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
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

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
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

/* Line 1455 of yacc.c  */

    { strncpy((yyval.string), (yyvsp[(1) - (1)].string), MAXLEN); ;}
    break;

  case 3:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),var_decl_begin((yyvsp[(1) - (1)].number))); ;}
    break;

  case 4:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (4)]),(yylsp[(4) - (4)]),var_decl_done()); ;}
    break;

  case 5:

/* Line 1455 of yacc.c  */

    { PERROR(PE_VARDECL); CALL((yylsp[(1) - (2)]),(yylsp[(1) - (2)]),var_decl_cancel()); ;}
    break;

  case 7:

/* Line 1455 of yacc.c  */

    { (yyval.number) = false; ;}
    break;

  case 8:

/* Line 1455 of yacc.c  */

    { (yyval.number) = true; ;}
    break;

  case 9:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(1) - (2)]),type_is_const((yyvsp[(1) - (2)].number))); (yyval.number) = (yyvsp[(2) - (2)].number); ;}
    break;

  case 10:

/* Line 1455 of yacc.c  */

    { PERROR(PE_VARDECL); ;}
    break;

  case 11:

/* Line 1455 of yacc.c  */

    { (yyval.number) = T_INT; ;}
    break;

  case 12:

/* Line 1455 of yacc.c  */

    { (yyval.number) = T_BYTE; ;}
    break;

  case 15:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]), (yylsp[(3) - (3)]), var_decl_create((yyvsp[(1) - (3)].string), (yyvsp[(2) - (3)].number), (yyvsp[(3) - (3)].flag))); ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */

    { (yyval.flag) = false; ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),take_expression()); ;}
    break;

  case 18:

/* Line 1455 of yacc.c  */

    { (yyval.flag) = true; ;}
    break;

  case 19:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(1) - (2)]),take_expression_cancel()); PERROR(PE_VARINIT); ;}
    break;

  case 20:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),var_init_is_field(false)); ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),var_init_is_field(true)); ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */

    {;}
    break;

  case 23:

/* Line 1455 of yacc.c  */

    {;}
    break;

  case 24:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),var_init_field_part()); ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */

    { (yyval.number) = 0; ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */

    { (yyval.number) = 1; CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),var_decl_array_size((yyvsp[(2) - (3)].number))); ;}
    break;

  case 27:

/* Line 1455 of yacc.c  */

    { parser->done(); ;}
    break;

  case 28:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(2) - (2)]),proc_decl_begin((yyvsp[(2) - (2)].string))); ;}
    break;

  case 29:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (6)]),(yylsp[(6) - (6)]),proc_decl_done()); ;}
    break;

  case 32:

/* Line 1455 of yacc.c  */

    {;}
    break;

  case 33:

/* Line 1455 of yacc.c  */

    { PERROR(PE_LOCALDECL); ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(2) - (3)]),(yylsp[(2) - (3)]),state_list_done()); ;}
    break;

  case 37:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),state_decl((yyvsp[(1) - (1)].string))); ;}
    break;

  case 38:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(2) - (3)]),(yylsp[(2) - (3)]),state_init((yyvsp[(2) - (3)].string))); ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(2) - (2)]),accept_type(T_BUCHI)); ;}
    break;

  case 47:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(2) - (3)]),accept_type(T_BUCHI)); ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(2) - (3)]),accept_type(T_GENBUCHI)); ;}
    break;

  case 49:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(2) - (3)]),accept_type(T_MULLER)); ;}
    break;

  case 50:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(2) - (3)]),accept_type(T_RABIN)); ;}
    break;

  case 51:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(2) - (3)]),accept_type(T_STREETT)); ;}
    break;

  case 52:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(1) - (2)]),state_accept((yyvsp[(1) - (2)].string))); ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(1) - (3)]),state_accept((yyvsp[(1) - (3)].string))); ;}
    break;

  case 54:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(1) - (2)]),accept_genbuchi_muller_set_complete()); ;}
    break;

  case 55:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(1) - (3)]),accept_genbuchi_muller_set_complete()); ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),state_genbuchi_muller_accept((yyvsp[(1) - (1)].string))); ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(1) - (3)]),state_genbuchi_muller_accept((yyvsp[(1) - (3)].string))); ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(1) - (2)]),accept_rabin_streett_pair_complete()); ;}
    break;

  case 61:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(1) - (3)]),accept_rabin_streett_pair_complete()); ;}
    break;

  case 63:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(2) - (2)]),accept_rabin_streett_first_complete()); ;}
    break;

  case 64:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(2) - (3)]),accept_rabin_streett_first_complete()); ;}
    break;

  case 67:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),state_rabin_streett_accept((yyvsp[(1) - (1)].string))); ;}
    break;

  case 68:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(1) - (3)]),state_rabin_streett_accept((yyvsp[(1) - (3)].string))); ;}
    break;

  case 70:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(1) - (2)]),state_commit((yyvsp[(1) - (2)].string))); ;}
    break;

  case 71:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(1) - (3)]),state_commit((yyvsp[(1) - (3)].string))); ;}
    break;

  case 76:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(1) - (2)]),take_expression()); ;}
    break;

  case 77:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (4)]),(yylsp[(3) - (4)]),assertion_create((yyvsp[(1) - (4)].string))); ;}
    break;

  case 78:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(1) - (3)]),take_expression_cancel()); PERROR(PE_ASSERT); ;}
    break;

  case 83:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (8)]),(yylsp[(8) - (8)]),trans_create((yyvsp[(1) - (8)].string),(yyvsp[(3) - (8)].string),(yyvsp[(5) - (8)].flag),(yyvsp[(6) - (8)].number),(yyvsp[(7) - (8)].number))); ;}
    break;

  case 85:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (5)]),(yylsp[(5) - (5)]),prob_trans_create((yyvsp[(1) - (5)].string))); ;}
    break;

  case 86:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),prob_transition_part((yyvsp[(1) - (3)].string),(yyvsp[(3) - (3)].number))); ;}
    break;

  case 89:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (7)]),(yylsp[(7) - (7)]),trans_create(0,(yyvsp[(2) - (7)].string),(yyvsp[(4) - (7)].flag),(yyvsp[(5) - (7)].number),(yyvsp[(6) - (7)].number))); ;}
    break;

  case 91:

/* Line 1455 of yacc.c  */

    { (yyval.flag) = false ;}
    break;

  case 92:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),take_expression()); ;}
    break;

  case 93:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(2) - (4)]),(yylsp[(2) - (4)]),trans_guard_expr()); (yyval.flag) = true; ;}
    break;

  case 94:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(1) - (2)]),take_expression_cancel()) ;}
    break;

  case 95:

/* Line 1455 of yacc.c  */

    { (yyval.flag) = false; ;}
    break;

  case 96:

/* Line 1455 of yacc.c  */

    { (yyval.number) = 0; ;}
    break;

  case 97:

/* Line 1455 of yacc.c  */

    { (yyval.number) = (yyvsp[(2) - (3)].number); ;}
    break;

  case 98:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(2) - (2)]),take_expression()); ;}
    break;

  case 99:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (4)]),(yylsp[(4) - (4)]),trans_sync((yyvsp[(1) - (4)].string),1,(yyvsp[(4) - (4)].flag))); (yyval.number) = 1; ;}
    break;

  case 100:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(2) - (3)]),take_expression_cancel()); ;}
    break;

  case 101:

/* Line 1455 of yacc.c  */

    { (yyval.number) = 1; ;}
    break;

  case 102:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(2) - (2)]),take_expression()); ;}
    break;

  case 103:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (4)]),(yylsp[(4) - (4)]),trans_sync((yyvsp[(1) - (4)].string),2,(yyvsp[(4) - (4)].flag))); (yyval.number) = 2; ;}
    break;

  case 104:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(2) - (3)]),take_expression_cancel()); ;}
    break;

  case 105:

/* Line 1455 of yacc.c  */

    { (yyval.number) = 1; ;}
    break;

  case 106:

/* Line 1455 of yacc.c  */

    { (yyval.flag) = false; ;}
    break;

  case 107:

/* Line 1455 of yacc.c  */

    { (yyval.flag) = true; ;}
    break;

  case 108:

/* Line 1455 of yacc.c  */

    { (yyval.flag) = true; ;}
    break;

  case 111:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]), expression_list_store()); ;}
    break;

  case 112:

/* Line 1455 of yacc.c  */

    { (yyval.number) = 0; ;}
    break;

  case 113:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),trans_effect_list_begin()); ;}
    break;

  case 114:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (4)]),(yylsp[(1) - (4)]),trans_effect_list_end()); ;}
    break;

  case 115:

/* Line 1455 of yacc.c  */

    { (yyval.number) = (yyvsp[(3) - (5)].number); ;}
    break;

  case 116:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(1) - (2)]),trans_effect_list_cancel());
	    PERROR(PE_EFFECT_LIST);
	  ;}
    break;

  case 117:

/* Line 1455 of yacc.c  */

    { (yyval.number) = 1; CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),trans_effect_part()); ;}
    break;

  case 118:

/* Line 1455 of yacc.c  */

    { (yyval.number) = (yyvsp[(1) - (3)].number) + 1; CALL((yylsp[(3) - (3)]),(yylsp[(3) - (3)]),trans_effect_part()); ;}
    break;

  case 119:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),expr_false()); ;}
    break;

  case 120:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),expr_true()); ;}
    break;

  case 121:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),expr_nat((yyvsp[(1) - (1)].number))); ;}
    break;

  case 122:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (1)]),(yylsp[(1) - (1)]),expr_id((yyvsp[(1) - (1)].string))); ;}
    break;

  case 123:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (4)]),(yylsp[(4) - (4)]),expr_array_mem((yyvsp[(1) - (4)].string))); ;}
    break;

  case 124:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_parenthesis()); ;}
    break;

  case 125:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (2)]),(yylsp[(2) - (2)]),expr_unary((yyvsp[(1) - (2)].number))); ;}
    break;

  case 126:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_LT)); ;}
    break;

  case 127:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_LEQ)); ;}
    break;

  case 128:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_EQ)); ;}
    break;

  case 129:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_NEQ)); ;}
    break;

  case 130:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_GT)); ;}
    break;

  case 131:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_GEQ)); ;}
    break;

  case 132:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_PLUS)); ;}
    break;

  case 133:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_MINUS)); ;}
    break;

  case 134:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_MULT)); ;}
    break;

  case 135:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_DIV)); ;}
    break;

  case 136:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_MOD)); ;}
    break;

  case 137:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_AND)); ;}
    break;

  case 138:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_OR)); ;}
    break;

  case 139:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_XOR)); ;}
    break;

  case 140:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_LSHIFT)); ;}
    break;

  case 141:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_RSHIFT)); ;}
    break;

  case 142:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_BOOL_AND)); ;}
    break;

  case 143:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_BOOL_OR)); ;}
    break;

  case 144:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_state_of_process((yyvsp[(1) - (3)].string),(yyvsp[(3) - (3)].string))); ;}
    break;

  case 145:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_var_of_process((yyvsp[(1) - (3)].string),(yyvsp[(3) - (3)].string))); ;}
    break;

  case 146:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (6)]),(yylsp[(3) - (6)]),expr_var_of_process((yyvsp[(1) - (6)].string),(yyvsp[(3) - (6)].string),true)); ;}
    break;

  case 147:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_bin(T_IMPLY)); ;}
    break;

  case 148:

/* Line 1455 of yacc.c  */

    { CALL((yylsp[(1) - (3)]),(yylsp[(3) - (3)]),expr_assign((yyvsp[(2) - (3)].number))); ;}
    break;

  case 149:

/* Line 1455 of yacc.c  */

    { PERROR(PE_EXPR); ;}
    break;

  case 150:

/* Line 1455 of yacc.c  */

    { (yyval.number) = T_ASSIGNMENT; ;}
    break;

  case 151:

/* Line 1455 of yacc.c  */

    { (yyval.number) = T_UNARY_MINUS; ;}
    break;

  case 152:

/* Line 1455 of yacc.c  */

    { (yyval.number) = T_TILDE; ;}
    break;

  case 153:

/* Line 1455 of yacc.c  */

    { (yyval.number) = T_BOOL_NOT; ;}
    break;



/* Line 1455 of yacc.c  */

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
      /* If just tried and failed to reuse lookahead token after an
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

  /* Else will try to reuse lookahead token after shifting the error
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

  *++yyvsp = yylval;

  yyerror_range[1] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the lookahead.  YYLOC is available though.  */
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

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
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



/* Line 1675 of yacc.c  */




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



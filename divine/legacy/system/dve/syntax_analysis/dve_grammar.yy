/* C++ declarations */
/* error type 351 */
/* error space 34000-34699 */

%{
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
%}

/* end of C++ declarations */

/* BEGIN OF TOKENS */
%token T_NO_TOKEN /* auxiliary constant used in expression_t */
/* Auxiliary symbols used for expressions in property process - they
 * enable property process to access variables and arrays of variables
 * in arbitrary process using construct "process->variable".
 */
%token T_FOREIGN_ID T_FOREIGN_SQUARE_BRACKETS

%token T_EXCLAM

/* Variable type declaration keywords: */
%token T_CONST T_TRUE T_FALSE

/* keywords */
%token T_SYSTEM T_PROCESS T_STATE T_CHANNEL T_COMMIT T_INIT T_ACCEPT T_ASSERT T_TRANS 
%token T_GUARD T_SYNC T_ASYNC T_PROPERTY T_EFFECT /* assign */
%token T_BUCHI T_GENBUCHI T_STREETT T_RABIN T_MULLER /* types of automata */

/* Miscelanious: */
%token <string>T_ID
%token T_INT T_BYTE
%token <number>T_NAT
%token T_WIDE_ARROW

/* Syntax switch tokens */
%token T_PROPERTY

%type <number> TypeName
%type <number> TypeId
%type <number> ArrayDecl
%type <number> FieldInitList
%type <number> UnaryOp
%type <number> AssignOp Const
%type <string> Id
%type <flag> VarInit
%type <number> Sync SyncExpr EffList Effect
%type <flag> Guard SyncValue ProcProperty

%right T_ASSIGNMENT 
%left T_IMPLY
%left T_BOOL_OR T_BOOL_AND 
%left T_OR T_AND T_XOR
%left T_EQ T_NEQ 
%left T_LT T_LEQ T_GEQ T_GT
%left T_LSHIFT T_RSHIFT
%left T_MINUS T_PLUS
%left T_MULT T_DIV T_MOD
%right T_TILDE T_BOOL_NOT T_UNARY_MINUS T_INCREMENT T_DECREMENT UOPERATOR
%left '(' ')' T_PARENTHESIS '[' ']' T_SQUARE_BRACKETS '.' T_DOT T_ARROW
/* T_SQUARE_BRACKETS, T_PARENTHESIS, T_DOT and T_PROCESS_CONTEXT are added as
 * auxiliary constants */

%union {
  bool flag;
  int number;
  char string[MAXLEN];
}

%start DVE

%%

/* END OF TOKENS */

/* BEGIN OF GRAMMAR RULES */


/* Top-most level */
DVE:
	Declaration ProcDeclList System { parser->done(); }
	;

Declaration:
	   /* empty */
	   | Declaration VariableDecl
	   | Declaration Channels
	   ;


Id:
	T_ID { strncpy($$, $1, MAXLEN); }
	;

/* Variable declarations */

VariableDecl:
	  TypeName { CALL(@1,@1,var_decl_begin($1)); } DeclIdList ';'
	  { CALL(@1,@4,var_decl_done()); }
	| TypeName error { PERROR(PE_VARDECL); CALL(@1,@1,var_decl_cancel()); } ';'
	;

Const:
	  /* empty */ { $$ = false; }
	| T_CONST { $$ = true; }
	;

TypeName:
	  Const TypeId { CALL(@1,@1,type_is_const($1)); $$ = $2; }
	| T_CONST error { PERROR(PE_VARDECL); }
	;

TypeId:
	T_INT { $$ = T_INT; }
	| T_BYTE { $$ = T_BYTE; }
	;
      

DeclIdList:
	DeclId
	| DeclIdList ',' DeclId
	;

DeclId:
	Id ArrayDecl VarInit { CALL(@1, @3, var_decl_create($1, $2, $3)); }  /* it can be also array - it is optional - see ArrayDecl */
	;

VarInit:
	/* empty */ { $$ = false; }
	| T_ASSIGNMENT { CALL(@1,@1,take_expression()); } Initializer { $$ = true; }
	| T_ASSIGNMENT error { CALL(@1,@1,take_expression_cancel()); PERROR(PE_VARINIT); }
	; 

Initializer:
	Expression { CALL(@1,@1,var_init_is_field(false)); }
	| '{' FieldInitList '}' { CALL(@1,@3,var_init_is_field(true)); }
	;

FieldInitList:
	FieldInit {}
	| FieldInitList ',' FieldInit {}
	;

FieldInit: 
	Expression  { CALL(@1,@1,var_init_field_part()); }
	;

ArrayDecl:  //returns number of array dimensions (0 => scalar)
	/* empty */ { $$ = 0; }
	| '[' T_NAT ']' { $$ = 1; CALL(@1,@3,var_decl_array_size($2)); }
	 /* only one-dimensional */
	;
	
/*********************************************************************
 * Process declaration
 */

ProcDeclList:
	ProcDecl ProcDeclList
	| ProcDecl
	;

/* non-terminal used for standalone parsing of processes */
ProcDeclStandalone:
	ProcDecl { parser->done(); }
	;

/* simple process declaration - no parameters */
ProcDecl:
	T_PROCESS Id { CALL(@1,@2,proc_decl_begin($2)); }
	'{' ProcBody '}' { CALL(@1,@6,proc_decl_done()); }
	;

/* The body of the process consists of local declarations, states,
   accepting and initial states and transitions.
*/
ProcBody:
	ProcLocalDeclList States InitAndCommitAndAccept Assertions Transitions 
	;

/* local declarations of variables */
ProcLocalDeclList:
	/* empty */
	| ProcLocalDeclList VariableDecl {}
	| ProcLocalDeclList error { PERROR(PE_LOCALDECL); }
	;


/* BEGIN of declarations of channels */
  /* Untyped channels */
Channels:
	T_CHANNEL ChannelDeclList ';'
	| T_CHANNEL '{' TypeList '}' TypedChannelDeclList ';' { CALL(@1,@5,type_list_clear()); }
	;

ChannelDeclList:
	ChannelDecl
	| ChannelDeclList ',' ChannelDecl
        ;

ChannelDecl:
	Id { CALL(@1,@1,channel_decl($1)); }
	;
  /* Typed channels */
TypedChannelDeclList:
	TypedChannelDecl
	| TypedChannelDeclList ',' TypedChannelDecl
        ;

TypedChannelDecl:
	Id '[' T_NAT ']' { CALL(@1,@4,typed_channel_decl($1,$3)); }
	;

/* END of declarations of channels */

/* BEGIN of list of types */

TypeList:
	OneTypeId
	| TypeList ',' OneTypeId
	;

OneTypeId:
	TypeId { CALL(@1,@1,type_list_add($1)); }
	;

/* END of list opf types */


/* BEGIN of declarations of states */
States:
	T_STATE StateDeclList ';' { CALL(@2,@2,state_list_done()); }
	;

StateDeclList:
	StateDecl
	| StateDeclList ',' StateDecl
	;

StateDecl:
	Id { CALL(@1,@1,state_decl($1)); }
	;
/* END of declarations of states */


/* BEGIN of declarations of initial and accepting states */
Init:
	T_INIT Id ';' { CALL(@2,@2,state_init($2)); }
	;

InitAndCommitAndAccept:
	Init
	|Init CommitAndAccept
	|CommitAndAccept Init
	;

CommitAndAccept:
	Commit Accept
	|Accept Commit
	|Accept
	|Commit
	;

Accept:
	T_ACCEPT AcceptListBuchi { CALL(@1,@2,accept_type(T_BUCHI)); }
	|T_ACCEPT T_BUCHI AcceptListBuchi { CALL(@1,@2,accept_type(T_BUCHI)); }
	|T_ACCEPT T_GENBUCHI AcceptListGenBuchiMuller { CALL(@1,@2,accept_type(T_GENBUCHI)); }
	|T_ACCEPT T_MULLER AcceptListGenBuchiMuller { CALL(@1,@2,accept_type(T_MULLER)); }
	|T_ACCEPT T_RABIN AcceptListRabinStreett { CALL(@1,@2,accept_type(T_RABIN)); }
	|T_ACCEPT T_STREETT AcceptListRabinStreett { CALL(@1,@2,accept_type(T_STREETT)); }
	;

AcceptListBuchi:
	Id ';' { CALL(@1,@1,state_accept($1)); }
	|Id ',' AcceptListBuchi { CALL(@1,@1,state_accept($1)); }
	;

AcceptListGenBuchiMuller:
	AcceptListGenBuchiMullerSet ';' { CALL(@1,@1,accept_genbuchi_muller_set_complete()); }
	|AcceptListGenBuchiMullerSet ',' AcceptListGenBuchiMuller { CALL(@1,@1,accept_genbuchi_muller_set_complete()); }
	;

AcceptListGenBuchiMullerSet:
	'(' AcceptListGenBuchiMullerSetNotEmpty ')'
	| '(' ')' /* empty set of acc. states */
	;

AcceptListGenBuchiMullerSetNotEmpty:
	Id { CALL(@1,@1,state_genbuchi_muller_accept($1)); }
	|Id ',' AcceptListGenBuchiMullerSetNotEmpty { CALL(@1,@1,state_genbuchi_muller_accept($1)); }
	;

AcceptListRabinStreett:
	AcceptListRabinStreettPair ';' { CALL(@1,@1,accept_rabin_streett_pair_complete()); }
	|AcceptListRabinStreettPair ',' AcceptListRabinStreett { CALL(@1,@1,accept_rabin_streett_pair_complete()); }
	;
	
AcceptListRabinStreettPair:
	AcceptListRabinStreettFirst AcceptListRabinStreettSecond
	;

AcceptListRabinStreettFirst:
	'(' ';' { CALL(@1,@2,accept_rabin_streett_first_complete()); }
	|'(' AcceptListRabinStreettSet ';' { CALL(@1,@2,accept_rabin_streett_first_complete()); }
	;

AcceptListRabinStreettSecond:
	')'
	| AcceptListRabinStreettSet ')'
	;

AcceptListRabinStreettSet:
	Id { CALL(@1,@1,state_rabin_streett_accept($1)); }
	| Id ',' AcceptListRabinStreettSet { CALL(@1,@1,state_rabin_streett_accept($1)); }
	;

Commit:
	T_COMMIT CommitList
	;

CommitList:
	Id ';' { CALL(@1,@1,state_commit($1)); }
	|Id ',' CommitList { CALL(@1,@1,state_commit($1)); }
	;

/* END of declarations of initial and accepting states */

/* BEGIN assertions definitions */

Assertions:
        /*empty*/
        | T_ASSERT AssertionList ';'
        ;

AssertionList:
        Assertion
        | AssertionList ',' Assertion
        ;

Assertion:
        Id ':' { CALL(@1,@1,take_expression()); } Expression { CALL(@1,@3,assertion_create($1)); }
        | Id ':' error { CALL(@1,@1,take_expression_cancel()); PERROR(PE_ASSERT); }
        ;

/* END assertions definitions */

/* BEGIN of definitions of transitions */
Transitions:
	/* empty */
	| T_TRANS TransitionList ';'
	;

TransitionList:
	Transition
	| TransitionList ',' TransitionOpt
	;

/* This non-terminal used for standalone parsing of transition */
TransitionStandalone:
	Transition { parser->done(); }
	;
	
Transition:
	Id T_ARROW Id '{' Guard Sync Effect '}'
	  { CALL(@1,@8,trans_create($1,$3,$5,$6,$7)); }
	| ProbTransition
	;

ProbTransition:
	Id T_WIDE_ARROW '{' ProbList '}' { CALL(@1,@5,prob_trans_create($1)); }
	;

ProbTransitionPart:
	Id ':' T_NAT { CALL(@1,@3,prob_transition_part($1,$3)); }
	;	

ProbList:
	ProbTransitionPart
	| ProbList ',' ProbTransitionPart
	;

TransitionOpt:
	T_ARROW Id '{' Guard Sync Effect '}'
	  { CALL(@1,@7,trans_create(0,$2,$4,$5,$6)); }
	| Transition
	;

Guard:
	/* empty */ { $$ = false }
        | T_GUARD { CALL(@1,@1,take_expression()); }
	  Expression ';' { CALL(@2,@2,trans_guard_expr()); $$ = true; }
	| T_GUARD error { CALL(@1,@1,take_expression_cancel()) } ';' { $$ = false; }
	;

Sync:
	/* empty */ { $$ = 0; }
	| T_SYNC SyncExpr ';' { $$ = $2; }
	;

SyncExpr:
	  Id T_EXCLAM { CALL(@1,@2,take_expression()); }
	  SyncValue { CALL(@1,@4,trans_sync($1,1,$4)); $$ = 1; }
	| Id T_EXCLAM error { CALL(@1,@2,take_expression_cancel()); } ';' { $$ = 1; }
	| Id '?' { CALL(@1,@2,take_expression()); }
	  SyncValue { CALL(@1,@4,trans_sync($1,2,$4)); $$ = 2; }
	| Id '?' error { CALL(@1,@2,take_expression_cancel()); } ';' { $$ = 1; }
	;

SyncValue:
	 /* empty */ { $$ = false; }
	| Expression { $$ = true; }
	| '{' ExpressionList '}' { $$ = true; }
	;

ExpressionList:
	OneExpression
	| ExpressionList ',' OneExpression
	;

OneExpression:
	Expression { CALL(@1,@1, expression_list_store()); }
	;

Effect:
	/* empty */ { $$ = 0; }
	| T_EFFECT { CALL(@1,@1,trans_effect_list_begin()); } EffList ';' { CALL(@1,@1,trans_effect_list_end()); }
	  { $$ = $3; }
	| T_EFFECT error
	  { CALL(@1,@1,trans_effect_list_cancel());
	    PERROR(PE_EFFECT_LIST);
	  }
	;

EffList:
	  Assignment { $$ = 1; CALL(@1,@1,trans_effect_part()); }
	| EffList ',' Assignment
	   { $$ = $1 + 1; CALL(@3,@3,trans_effect_part()); }
        ;

/* END of definitions of transitions */


/* BEGIN of definitions of expressions */

/* This non-terminal used during a standalone parsing of expression */
ExpressionStandalone:
	Expression { parser->done(); }
	;

Expression:
	T_FALSE                                               /* false */
	 { CALL(@1,@1,expr_false()); }    /* OP_ZERO */
        | T_TRUE                                              /* true */
	 { CALL(@1,@1,expr_true()); }     /* OP_ZERO */
	| T_NAT                                               /* cislo */
	 { CALL(@1,@1,expr_nat($1)); }      /* OP_ZERO */
	| Id                                                /* ID */
	 { CALL(@1,@1,expr_id($1)); }     /* OP_ZERO */
        | Id '[' Expression ']'     /* OP_ARRAYMEM */ /* ID[E] */
	 { CALL(@1,@4,expr_array_mem($1)); } 
	| '(' Expression ')'                                  /* (E) */
	 { CALL(@1,@3,expr_parenthesis()); }
	| UnaryOp Expression %prec UOPERATOR                  /* Un(E) */
	 { CALL(@1,@2,expr_unary($1)); }     
	| Expression T_LT Expression                          /* < */
	 { CALL(@1,@3,expr_bin(T_LT)); }
	| Expression T_LEQ Expression                         /* <= */
	 { CALL(@1,@3,expr_bin(T_LEQ)); }
	| Expression T_EQ Expression                          /* == */
	 { CALL(@1,@3,expr_bin(T_EQ)); }
	| Expression T_NEQ Expression                         /* != */
	 { CALL(@1,@3,expr_bin(T_NEQ)); }
	| Expression T_GT Expression                          /* > */
	 { CALL(@1,@3,expr_bin(T_GT)); }
	| Expression T_GEQ Expression                         /* >= */
	 { CALL(@1,@3,expr_bin(T_GEQ)); }
	| Expression T_PLUS Expression                        /* + */
	 { CALL(@1,@3,expr_bin(T_PLUS)); }
	| Expression T_MINUS Expression                       /* - */
	 { CALL(@1,@3,expr_bin(T_MINUS)); }
	| Expression T_MULT Expression                        /* * */
	 { CALL(@1,@3,expr_bin(T_MULT)); }
	| Expression T_DIV Expression                         /* / */
	 { CALL(@1,@3,expr_bin(T_DIV)); }
	| Expression T_MOD Expression                         /* % */
	 { CALL(@1,@3,expr_bin(T_MOD)); }
	| Expression T_AND Expression                           /* & */
	 { CALL(@1,@3,expr_bin(T_AND)); }
	| Expression T_OR Expression                          /* | */
	 { CALL(@1,@3,expr_bin(T_OR)); }
	| Expression T_XOR Expression                         /* ^ */
	 { CALL(@1,@3,expr_bin(T_XOR)); }
	| Expression T_LSHIFT Expression                      /* << */
	 { CALL(@1,@3,expr_bin(T_LSHIFT)); }
	| Expression T_RSHIFT Expression                      /* >> */
	 { CALL(@1,@3,expr_bin(T_RSHIFT)); }
	| Expression T_BOOL_AND Expression                    /* and */
	 { CALL(@1,@3,expr_bin(T_BOOL_AND)); }
	| Expression T_BOOL_OR Expression                     /* or */
	 { CALL(@1,@3,expr_bin(T_BOOL_OR)); }
        | Id '.' Id                                       /* ID.state */
	 { CALL(@1,@3,expr_state_of_process($1,$3)); }
	 /* evaluation of expression in a context of other process,
	    permitted only in property process */
	| Id T_ARROW Id                       /* ID->variable */
	 { CALL(@1,@3,expr_var_of_process($1,$3)); }
	| Id T_ARROW Id '[' Expression ']'                 /* ID->ID[E] */
	 { CALL(@1,@3,expr_var_of_process($1,$3,true)); }
	| Expression T_IMPLY Expression                       /* E implies E*/
	 { CALL(@1,@3,expr_bin(T_IMPLY)); }
	;


/* extension of expressions - there can be exactly one assignment */
Assignment:
          Expression AssignOp Expression %prec T_ASSIGNMENT   /* ID...=E */
	   { CALL(@1,@3,expr_assign($2)); }
	| Expression AssignOp error { PERROR(PE_EXPR); } %prec T_ASSIGNMENT
	;

AssignOp: 
	/* = += -= /= %= &= |= ^= <<= >>= */
	  T_ASSIGNMENT { $$ = T_ASSIGNMENT; }
	;
/* unary operators possible in expressions: */
UnaryOp: 
	/* - ! not */  
	T_MINUS { $$ = T_UNARY_MINUS; }
	| T_TILDE { $$ = T_TILDE; }           /* ~ = bitwise negation */
	| T_BOOL_NOT { $$ = T_BOOL_NOT; }     /* not = bool negation */
	;


/* BEGIN of definitions of type of system */
System:
	T_SYSTEM SystemType
	;

SystemType:
	  T_ASYNC /*ProcList*/ ProcProperty ';'
	  { CALL(@1,@3,system_synchronicity(1,$2)); }
	| T_SYNC /*ProcList*/ ProcProperty ';'
	  { CALL(@1,@1,system_synchronicity(0)); }
	;

ProcProperty:
	/* empty */ { $$ = false; }
	| T_PROPERTY Id { CALL(@2,@2,system_property($2)); $$ = true; }
	;

/* END of definitions of type of system */


%%


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


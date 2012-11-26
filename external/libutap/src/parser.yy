// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; -*-

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

/*********************************************************************
 * This bison grammar file contains both grammars for old and new XTA
 * formats plus entry points for various productions used in the XML
 * parser.
 * 
 * There are numerous problems with parser error recognition and
 * recovery in general, since the XTA language contains some tricky
 * properties:
 *
 * 1) The entity terminating tokens such as '}' and ';' are used for
 * multiple purposes, thus making it imposible to "skip until the end
 * of current entity".
 *
 * 2) Since entity terminating token is not always clear, there might
 * appear continuous error reports on the same code as no recovery is
 * made. Please report it, this might be corrected.
 */

%{

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

%}


/* Assignments: */
%token T_ASSIGNMENT T_ASSPLUS 
%token T_ASSMINUS T_ASSMULT T_ASSDIV T_ASSMOD 
%token T_ASSOR T_ASSAND T_ASSXOR 
%token T_ASSLSHIFT T_ASSRSHIFT

/* Unary operations: */
%token T_EXCLAM

%token T_INCREMENT T_DECREMENT

/* Binary operations: */
%token T_PLUS T_MINUS T_MULT T_DIV  T_INF T_SUP
%token T_MOD T_OR T_XOR T_LSHIFT T_RSHIFT 
%token T_BOOL_AND T_BOOL_OR 
%token T_KW_AND T_KW_OR T_KW_IMPLY T_KW_NOT

/* Quantifiers */
%token T_FORALL T_EXISTS

/* Relation operations:*/
%token T_LT T_LEQ T_EQ T_NEQ T_GEQ T_GT

/* Special statement keywords: */
%token T_FOR T_WHILE T_DO T_BREAK T_CONTINUE T_SWITCH T_IF T_ELSE 
%token T_CASE T_DEFAULT T_RETURN T_ASSERT
%token T_PRIORITY

/* Variable type declaration keywords: */
%token T_TYPEDEF T_STRUCT 
%token T_CONST T_OLDCONST T_URGENT T_BROADCAST T_TRUE T_FALSE T_META

/* Uppaal keywords */
%token T_SYSTEM T_PROCESS T_STATE T_COMMIT T_INIT T_TRANS T_SELECT
%token T_GUARD T_SYNC T_ASSIGN T_BEFORE T_AFTER T_PROGRESS
%token T_ARROW T_UNCONTROL_ARROW

/* Property tokens */
%token T_DEADLOCK T_EF T_EG T_AF T_AG T_LEADSTO T_RESULTSET
%token T_EF2 T_EG2 T_AF2 T_AG2

/* Control Synthesis */
%token T_CONTROL T_MIN_START T_MAX_START T_CONTROL_T

/* Miscelanious: */
%token T_ERROR 
%token <string>T_ID T_TYPENAME
%token <number>T_NAT

/* Types */
%token T_BOOL T_INT T_CHAN T_CLOCK T_VOID T_SCALAR

/* Syntax switch tokens */
%token T_NEW T_NEW_DECLARATION T_NEW_LOCAL_DECL T_NEW_INST T_NEW_SYSTEM 
%token T_NEW_PARAMETERS T_NEW_INVARIANT T_NEW_GUARD T_NEW_SYNC T_NEW_ASSIGN
%token T_NEW_SELECT
%token T_OLD T_OLD_DECLARATION T_OLD_LOCAL_DECL T_OLD_INST 
%token T_OLD_PARAMETERS T_OLD_INVARIANT T_OLD_GUARD T_OLD_ASSIGN
%token T_PROPERTY T_EXPRESSION 

%type <number> ArgList FieldDeclList FieldDeclIdList FieldDecl
%type <number> ParameterList FieldInitList SimpleIdList
%type <number> OptionalInstanceParameterList
%type <prefix> Type TypePrefix 
%type <string> Id NonTypeId
%type <kind> UnaryOp AssignOp
%type <flag> VarInit 

%left T_LEADSTO 
%right T_AF T_AG T_EF T_EG 'A'
%right T_AF2 T_AG2 T_EF2 T_EG2
%left 'U' 'W'
%right T_FORALL T_EXISTS
%left T_KW_OR T_KW_IMPLY
%left T_KW_AND
%right T_KW_NOT
%right T_ASSIGNMENT T_ASSPLUS T_ASSMINUS T_ASSMULT T_ASSDIV T_ASSMOD T_ASSAND T_ASSOR T_ASSLSHIFT T_ASSRSHIFT T_ASSXOR
%right '?' ':'
%left T_BOOL_OR
%left T_BOOL_AND 
%left T_OR
%left T_XOR
%left '&'
%left T_EQ T_NEQ 
%left T_LT T_LEQ T_GEQ T_GT
%left T_MIN T_MAX
%left T_LSHIFT T_RSHIFT
%left T_MINUS T_PLUS
%left T_MULT T_DIV T_MOD
%right T_EXCLAM T_INCREMENT T_DECREMENT UOPERATOR
%left '(' ')' '[' ']' '.' '\''

%union {
    bool flag;
    int number;
    ParserBuilder::PREFIX prefix;
    kind_t kind;
    char string[MAXLEN];
}

/* Expect 1 shift/reduce warning in dangling ELSE part of IF statement */
%expect 1

%%

Uppaal:
          T_NEW XTA { CALL(@2, @2, done()); }
        | T_NEW_DECLARATION Declarations { }
        | T_NEW_LOCAL_DECL ProcLocalDeclList { }
        | T_NEW_INST Declarations { }
        | T_NEW_SYSTEM XTA { }
        | T_NEW_PARAMETERS ParameterList { }
        | T_NEW_INVARIANT Expression { }
        | T_NEW_SELECT SelectList { }
        | T_NEW_GUARD Expression { CALL(@2, @2, procGuard()); }
        | T_NEW_SYNC SyncExpr { }
        | T_NEW_ASSIGN ExprList { CALL(@2, @2, procUpdate()); }
        | T_OLD OldXTA { CALL(@2, @2, done()); }
        | T_OLD_DECLARATION OldDeclaration { }
        | T_OLD_LOCAL_DECL OldVarDeclList { }
        | T_OLD_INST Instantiations { }
        | T_OLD_PARAMETERS OldProcParamList { }
        | T_OLD_INVARIANT OldInvariant { }
        | T_OLD_GUARD OldGuardList { CALL(@2, @2, procGuard()); }
        | T_OLD_ASSIGN ExprList { CALL(@2, @2, procUpdate()); }
        | T_PROPERTY PropertyList {}
        | T_EXPRESSION Expression {}
        ;

XTA: 
        Declarations System
        ;

Instantiations:
        /* empty */
        | Instantiations Instantiation
        | error ';'
        ;

Instantiation:
        NonTypeId OptionalInstanceParameterList T_ASSIGNMENT NonTypeId '(' {
          CALL(@1, @4, instantiationBegin($1, $2, $4));
        } ArgList ')' ';' {
          CALL(@1, @9, instantiationEnd($1, $2, $4, $7));
        };

OptionalInstanceParameterList:
          { $$ = 0; }
        | '(' ')' 
          {
        	$$ = 0; 
          }
        | '(' ParameterList ')'
          {
        	$$ = $2;
          };

System: SysDecl Progress;

PriorityDecl: 
        T_CHAN T_PRIORITY ChannelList ';'
        | T_CHAN T_PRIORITY error ';'
        ;

ChannelList:
        ChanElement
        | ChannelList ',' ChanElement
        | ChannelList ChanLessThan ChanElement
        ;

ChanLessThan:
        T_LT { CALL(@1, @1, incChanPriority()); };

ChanElement:
        /* Cannot use 'Expression', since we use '<' (T_LT) as a separator. */
        ChanExpression { CALL(@1, @1, chanPriority()); }
        | T_DEFAULT { CALL(@1, @1, defaultChanPriority()); }
        ;

ChanExpression:
        NonTypeId { CALL(@1, @1, exprId($1)); }
        | ChanExpression '[' Expression ']' { CALL(@1, @4, exprArray()); }
        ;

SysDecl:
        T_SYSTEM ProcessList ';'
        | T_SYSTEM error ';'
        ;

ProcessList:
        NonTypeId { CALL(@1, @1, process($1)); }
        | ProcessList ',' NonTypeId { CALL(@3, @3, process($3)); }
        | ProcessList ProcLessThan NonTypeId { CALL(@3, @3, process($3)); }
        ;

ProcLessThan:
        T_LT { CALL(@1, @1, incProcPriority()); };

Progress:
        /* empty */
        | T_PROGRESS '{' ProgressMeasureList  '}';

ProgressMeasureList:
        /* empty */
        | ProgressMeasureList Expression ':' Expression ';' {
            CALL(@2, @4, declProgress(true));
        }
        | ProgressMeasureList Expression ';' {
            CALL(@2, @2, declProgress(false));
        };

Declarations:
        /* empty */
        | Declarations FunctionDecl
        | Declarations VariableDecl
        | Declarations TypeDecl
        | Declarations ProcDecl
        | Declarations BeforeUpdateDecl
        | Declarations AfterUpdateDecl
        | Declarations Instantiation
        | Declarations PriorityDecl
        | error ';'
        ;

BeforeUpdateDecl: T_BEFORE '{' ExprList '}' { CALL(@3, @3, beforeUpdate()); };

AfterUpdateDecl: T_AFTER '{' ExprList '}' { CALL(@3, @3, afterUpdate()); };

FunctionDecl:
        /* Notice that StatementList will catch all errors. Hence we
         * should be able to guarantee, that once declFuncBegin() has
         * been called, we will also call declFuncEnd().
         */
        Type Id OptionalParameterList '{' { 
          CALL(@1, @2, declFuncBegin($2));
        } BlockLocalDeclList StatementList '}' {
          CALL(@8, @8, declFuncEnd());
        };

OptionalParameterList:
          '(' ')' 
        | '(' ParameterList ')'
        | '(' error ')' 
        ;

ParameterList:
          Parameter { $$ = 1; }
        | ParameterList ',' Parameter { $$ = $1+1; }
        ;

Parameter:
          Type '&' NonTypeId ArrayDecl {
          CALL(@1, @4, declParameter($3, true));
        }
        | Type NonTypeId ArrayDecl {
          CALL(@1, @3, declParameter($2, false));
        }
        ;

VariableDecl:
        Type DeclIdList ';' { 
            CALL(@1, @3, typePop());
        }
        ;

DeclIdList:
        DeclId
        | DeclIdList ',' DeclId
        ;

DeclId:
        Id {
            CALL(@1, @1, typeDuplicate());
        } ArrayDecl VarInit { 
            CALL(@1, @4, declVar($1, $4));
        }
        ;

VarInit:
        /* empty */ { $$ = false; }
        | T_ASSIGNMENT Initializer { $$ = true; }
        ; 

Initializer:
        Expression
        | '{' FieldInitList '}' {	
          CALL(@1, @3, declInitialiserList($2));
        }
        ;

FieldInitList:
        FieldInit { $$ = 1; }
        | FieldInitList ',' FieldInit { $$ = $1+1; }
        ;

FieldInit:
        Id ':' Initializer { 
          CALL(@1, @3, declFieldInit($1));
        }
        | Initializer { 
          CALL(@1, @1, declFieldInit(""));
        }
        ;

ArrayDecl:
        { types = 0; } ArrayDecl2;

ArrayDecl2:
        /* empty */ 
        | '[' Expression ']'        ArrayDecl2 { CALL(@1, @3, typeArrayOfSize(types)); }
        | '[' Type ']' { types++; } ArrayDecl2 { CALL(@1, @3, typeArrayOfType(types--)); }
        | '[' error ']' ArrayDecl2
        ;

TypeDecl:
        T_TYPEDEF Type TypeIdList ';' {
          CALL(@1, @4, typePop());
        }
        | T_TYPEDEF error ';'
        ;

TypeIdList:
        TypeId
        | TypeIdList ',' TypeId 
        ;

TypeId:
        Id {
            CALL(@1, @1, typeDuplicate());
        } ArrayDecl { 
            CALL(@1, @3, declTypeDef($1));
        }
        ;

Type: 
        T_TYPENAME { 
            CALL(@1, @1, typeName(ParserBuilder::PREFIX_NONE, $1));
        }
        | TypePrefix T_TYPENAME { 
            CALL(@1, @2, typeName($1, $2));
        }
        | T_STRUCT '{' FieldDeclList '}' { 
            CALL(@1, @4, typeStruct(ParserBuilder::PREFIX_NONE, $3));
        }
        | TypePrefix T_STRUCT '{' FieldDeclList '}' { 
            CALL(@1, @5, typeStruct($1, $4));
        }
        | T_STRUCT '{' error '}' { 
          CALL(@1, @4, typeStruct(ParserBuilder::PREFIX_NONE, 0));
        }
        | TypePrefix T_STRUCT '{' error '}' { 
          CALL(@1, @5, typeStruct(ParserBuilder::PREFIX_NONE, 0));
        }
        | T_BOOL {
          CALL(@1, @1, typeBool(ParserBuilder::PREFIX_NONE));
        }
        | TypePrefix T_BOOL {
          CALL(@1, @2, typeBool($1));
        }
        | T_INT {
          CALL(@1, @1, typeInt(ParserBuilder::PREFIX_NONE));
        }
        | TypePrefix T_INT {
          CALL(@1, @2, typeInt($1));
        }
        | T_INT '[' Expression ',' Expression ']' 
        {
          CALL(@1, @6, typeBoundedInt(ParserBuilder::PREFIX_NONE));
        }
        | TypePrefix T_INT  '[' Expression ',' Expression ']' {
          CALL(@1, @7, typeBoundedInt($1));
        }
        | T_CHAN {
          CALL(@1, @1, typeChannel(ParserBuilder::PREFIX_NONE));
        }
        | TypePrefix T_CHAN {
          CALL(@1, @2, typeChannel($1));
        }
        | T_CLOCK {
          CALL(@1, @1, typeClock());
        }
        | T_VOID {
          CALL(@1, @1, typeVoid());
        }
        | T_SCALAR '[' Expression ']' 
        {
          CALL(@1, @4, typeScalar(ParserBuilder::PREFIX_NONE));
        }
        | TypePrefix T_SCALAR  '[' Expression ']' {
          CALL(@1, @5, typeScalar($1));
        }
        ;

Id:
        NonTypeId { strncpy($$, $1, MAXLEN); }
        | T_TYPENAME { strncpy($$, $1, MAXLEN); }
        ;

NonTypeId:
        T_ID  { strncpy($$, $1 , MAXLEN); }
        | 'A' { strncpy($$, "A", MAXLEN); }
        | 'U' { strncpy($$, "U", MAXLEN); }
        | 'W' { strncpy($$, "W", MAXLEN); }
        | T_INF { strncpy($$, "inf", MAXLEN); }
        | T_SUP { strncpy($$, "sup", MAXLEN); }
        ;

FieldDeclList:
        FieldDecl { $$=$1; }
        | FieldDeclList FieldDecl { $$=$1+$2; }
        ;

FieldDecl:
        Type FieldDeclIdList ';' {
          $$ = $2; 
          CALL(@1, @3, typePop());
        }
        ;

FieldDeclIdList:
        FieldDeclId { $$=1; }
        | FieldDeclIdList ',' FieldDeclId { $$=$1+1; }
        ;

FieldDeclId:
        Id {
            CALL(@1, @1, typeDuplicate());
        } ArrayDecl { 
            CALL(@1, @3, structField($1));
        }
        ;

TypePrefix:
          T_URGENT    { $$ = ParserBuilder::PREFIX_URGENT; }
        | T_BROADCAST { $$ = ParserBuilder::PREFIX_BROADCAST; }
        | T_URGENT T_BROADCAST { $$ = ParserBuilder::PREFIX_URGENT_BROADCAST; }
        | T_CONST  { $$ = ParserBuilder::PREFIX_CONST; } 
        | T_META { $$ = ParserBuilder::PREFIX_META; }
        ;
        
/*********************************************************************
 * Process declaration
 */

ProcDecl:
        T_PROCESS Id OptionalParameterList '{' { 
          CALL(@1, @4, procBegin($2));
        } 
        ProcBody '}' { 
          CALL(@6, @7, procEnd());
        }
        ;

ProcBody:
        ProcLocalDeclList States LocFlags Init Transitions 
        ;

ProcLocalDeclList:
        /* empty */
        | ProcLocalDeclList FunctionDecl
        | ProcLocalDeclList VariableDecl
        | ProcLocalDeclList TypeDecl
        ;

States:
        T_STATE StateDeclList ';'
        | error ';'
        ;

StateDeclList:
        StateDecl
        | StateDeclList ',' StateDecl
        ;

StateDecl:
        NonTypeId { 
          CALL(@1, @1, procState($1, false));
        }
        | NonTypeId '{' Expression '}' { 
          CALL(@1, @4, procState($1, true));
        }
        | NonTypeId '{' error '}' { 
          CALL(@1, @4, procState($1, false));
        }
        ;

Init:
        T_INIT NonTypeId ';' { 
          CALL(@1, @3, procStateInit($2));
        }
        | error ';' 
        ;

Transitions:
        /* empty */
        | T_TRANS TransitionList ';'
        | T_TRANS error ';' 
        ;

TransitionList:
        Transition
        | TransitionList ',' TransitionOpt
        ;

Transition:
        NonTypeId T_ARROW NonTypeId '{' {
            CALL(@1, @3, procEdgeBegin($1, $3, true));
        } Select Guard Sync Assign '}' { 
          strcpy(rootTransId, $1); 
          CALL(@1, @8, procEdgeEnd($1, $3));
        }
        | NonTypeId T_UNCONTROL_ARROW NonTypeId '{' {
            CALL(@1, @3, procEdgeBegin($1, $3, false));
        } Select Guard Sync Assign '}' { 
          strcpy(rootTransId, $1); 
          CALL(@1, @8, procEdgeEnd($1, $3));
        }
        ;

TransitionOpt:
        T_ARROW NonTypeId '{' { 
            CALL(@1, @2, procEdgeBegin(rootTransId, $2, true)); 
        } Select Guard Sync Assign '}' { 
            CALL(@1, @7, procEdgeEnd(rootTransId, $2));
        }
        | T_UNCONTROL_ARROW NonTypeId '{' { 
            CALL(@1, @2, procEdgeBegin(rootTransId, $2, false)); 
        } Select Guard Sync Assign '}' { 
            CALL(@1, @7, procEdgeEnd(rootTransId, $2));
        }
        | Transition
        ;

Select:
        /* empty */
        | T_SELECT SelectList ';'
        ;

SelectList:
        Id ':' Type {
            CALL(@1, @3, procSelect($1));
        }
        | SelectList ',' Id ':' Type {
            CALL(@3, @5, procSelect($3));
        }

Guard:
        /* empty */ 
        | T_GUARD Expression ';' {
          CALL(@2, @2, procGuard());
        }
        | T_GUARD Expression error ';' {
          CALL(@2, @3, procGuard());
        }
        | T_GUARD error ';'
        ;

Sync:
        /* empty */ 
        | T_SYNC SyncExpr ';'
        | T_SYNC error ';' 
        ;

SyncExpr:
        Expression T_EXCLAM { 
          CALL(@1, @2, procSync(SYNC_BANG));
        }
        | Expression error T_EXCLAM { 
          CALL(@1, @2, procSync(SYNC_QUE));
        }
        | Expression '?' { 
          CALL(@1, @2, procSync(SYNC_QUE));
        }
        | Expression error '?' { 
          CALL(@1, @2, procSync(SYNC_QUE));
        }
        ;

Assign:
        /* empty */ 
        | T_ASSIGN ExprList ';' {
          CALL(@2, @2, procUpdate());	  
        }
        | T_ASSIGN error ';' 
        ;

LocFlags:
        /* empty */
        | LocFlags Commit
        | LocFlags Urgent
        ;

Commit:
        T_COMMIT CStateList ';'
        | T_COMMIT error ';' 
        ;

Urgent:
        T_URGENT UStateList ';'
        | T_URGENT error ';' 
        ;

CStateList:
        NonTypeId { 
          CALL(@1, @1, procStateCommit($1));
        }
        | CStateList ',' NonTypeId { 
          CALL(@1, @3, procStateCommit($3));
        }
        ;

UStateList:
        NonTypeId { 
          CALL(@1, @1, procStateUrgent($1));
        }
        | UStateList ',' NonTypeId { 
          CALL(@1, @3, procStateUrgent($3));
        }
        ;

/**********************************************************************
 * Uppaal C grammar
 */

Block:
        '{' { 
          CALL(@1, @1, blockBegin());
        }
        BlockLocalDeclList StatementList '}' { 
          CALL(@2, @4, blockEnd());
        }
        ;

BlockLocalDeclList:
        /* empty */
        | BlockLocalDeclList VariableDecl
        | BlockLocalDeclList TypeDecl
        ;

StatementList:
        /* empty */
        | StatementList Statement
        | StatementList error ';'
        ;

Statement:
        Block
        | ';' { 
          CALL(@1, @1, emptyStatement());
        }
        | Expression ';' { 
          CALL(@1, @2, exprStatement());
        }
        | T_FOR '(' ExprList ';' ExprList ';' ExprList ')' { 
          CALL(@1, @8, forBegin());
        } 
        Statement { 
            CALL(@9, @9, forEnd());
        }
        | T_FOR '(' Id ':' Type ')' { 
            CALL(@1, @6, iterationBegin($3));
        } 
        Statement { 
            CALL(@7, @7, iterationEnd($3));
        }
        | T_FOR '(' error ')' Statement
        | T_WHILE '(' {
            CALL(@1, @2, whileBegin());
        }
          ExprList ')' Statement { 
            CALL(@3, @4, whileEnd());
          }
        | T_WHILE '(' error ')' Statement 
        | T_DO { 
            CALL(@1, @1, doWhileBegin());
        }
          Statement T_WHILE '(' ExprList ')' ';' { 
            CALL(@2, @7, doWhileEnd());
          }
        | T_IF '(' { 
            CALL(@1, @2, ifBegin());
        }
          ExprList ')' Statement ElsePart
        | T_BREAK ';' { 
            CALL(@1, @2, breakStatement());
          }
        | T_CONTINUE ';' { 
          CALL(@1, @2, continueStatement());
        }
        | T_SWITCH '(' ExprList ')' { 
            CALL(@1, @4, switchBegin());
        }
           '{' SwitchCaseList '}' { 
               CALL(@5, @7, switchEnd());
           }
        | T_RETURN Expression ';' { 
          CALL(@1, @3, returnStatement(true));
        }
        | T_RETURN ';' { 
          CALL(@1, @2, returnStatement(false));
        }
        | T_ASSERT Expression ';' {
	    CALL(@1, @2, assertStatement());
	}
        ;

ElsePart: 
        /* empty */ { 
          CALL(position_t(), position_t(), ifEnd(false));
        }
        | T_ELSE { 
          CALL(@1, @1, ifElse());
        }
          Statement { 
          CALL(@2, @2, ifEnd(true));
          }
        ;

SwitchCaseList:
        SwitchCase
        | SwitchCaseList SwitchCase
        ;

SwitchCase:
        T_CASE Expression ':' { 
	    CALL(@1, @3, caseBegin());
        }
        StatementList { 
            CALL(@4, @4, caseEnd());
	}
        | T_CASE error ':' { 
	    CALL(@1, @3, caseBegin());
        }
        StatementList {
            CALL(@4, @4, caseEnd());
	}
        | T_DEFAULT ':' { 
            CALL(@1, @2, defaultBegin());
        }
        StatementList { 
            CALL(@3, @3, defaultEnd());
        }
        | T_DEFAULT error ':' { 
	    CALL(@1, @3, defaultBegin());
        }
        StatementList { 
            CALL(@4, @4, defaultEnd());
        }
        ;

ExprList:
          Expression
        | ExprList ',' Expression { 
          CALL(@1, @3, exprComma());
        };

Expression:
        T_FALSE { 
          CALL(@1, @1, exprFalse());
        }
        | T_TRUE { 
          CALL(@1, @1, exprTrue());
        }
        | T_NAT  { 
          CALL(@1, @1, exprNat($1));
        }
        | NonTypeId { 
          CALL(@1, @1, exprId($1));
        }
        | Expression '(' {
            CALL(@1, @2, exprCallBegin());	    
          } ArgList ')'  { 
            CALL(@1, @5, exprCallEnd($4));
        }
        | Expression '(' {
            CALL(@1, @2, exprCallBegin());
          } error ')' {   
            CALL(@1, @5, exprCallEnd(0));
        }
        | Expression '[' Expression ']' { 
          CALL(@1, @4, exprArray());
        }
        | Expression '[' error ']' { 
          CALL(@1, @4, exprFalse());
        }
        | '(' Expression ')'
        | '(' error ')' {
          CALL(@1, @3, exprFalse());
        }
        | Expression T_INCREMENT { 
          CALL(@1, @2, exprPostIncrement());
        }
        | T_INCREMENT Expression { 
          CALL(@1, @2, exprPreIncrement());
        }
        | Expression T_DECREMENT { 
          CALL(@1, @2, exprPostDecrement());
        }
        | T_DECREMENT Expression { 
          CALL(@1, @2, exprPreDecrement());
        }
        | UnaryOp Expression { 
          CALL(@1, @2, exprUnary($1));
        } %prec UOPERATOR
        | Expression T_LT Expression { 
          CALL(@1, @3, exprBinary(LT));
        }
        | Expression T_LEQ Expression { 
          CALL(@1, @3, exprBinary(LE));
        }
        | Expression T_EQ Expression { 
          CALL(@1, @3, exprBinary(EQ));
        }
        | Expression T_NEQ Expression { 
          CALL(@1, @3, exprBinary(NEQ));
        }
        | Expression T_GT Expression { 
          CALL(@1, @3, exprBinary(GT));
        }
        | Expression T_GEQ Expression { 
          CALL(@1, @3, exprBinary(GE));
        }
        | Expression T_PLUS Expression { 
          CALL(@1, @3, exprBinary(PLUS));
        }
        | Expression T_MINUS Expression { 
          CALL(@1, @3, exprBinary(MINUS));
        }
        | Expression T_MULT Expression { 
          CALL(@1, @3, exprBinary(MULT));
        }
        | Expression T_DIV Expression { 
          CALL(@1, @3, exprBinary(DIV));
        }
        | Expression T_MOD Expression { 
          CALL(@1, @3, exprBinary(MOD));
        }
        | Expression '&'  Expression { 
          CALL(@1, @3, exprBinary(BIT_AND));
        }
        | Expression T_OR Expression { 
          CALL(@1, @3, exprBinary(BIT_OR));
        }
        | Expression T_XOR Expression { 
          CALL(@1, @3, exprBinary(BIT_XOR));
        }
        | Expression T_LSHIFT Expression { 
          CALL(@1, @3, exprBinary(BIT_LSHIFT));
        }
        | Expression T_RSHIFT Expression { 
          CALL(@1, @3, exprBinary(BIT_RSHIFT));
        }
        | Expression T_BOOL_AND Expression { 
          CALL(@1, @3, exprBinary(AND));
        }
        | Expression T_BOOL_OR Expression { 
          CALL(@1, @3, exprBinary(OR));
        }
        | Expression '?' Expression ':' Expression { 
          CALL(@1, @5, exprInlineIf());
        }
        | Expression '.' NonTypeId { 
          CALL(@1, @3, exprDot($3));
        }
        | Expression '\'' {
            CALL(@1, @2, exprUnary(RATE));
        }
        | T_DEADLOCK {
          CALL(@1, @1, exprDeadlock());
        }
        | Expression T_KW_IMPLY {  
          CALL(@1, @1, exprUnary(NOT));
        } Expression {
          CALL(@3, @3, exprBinary(OR));
        }
        | Expression T_KW_AND Expression {
          CALL(@1, @3, exprBinary(AND));
        }
        | Expression T_KW_OR Expression {
          CALL(@1, @3, exprBinary(OR));
        }
        | T_KW_NOT Expression {
          CALL(@1, @2, exprUnary(NOT));
        }
        | Expression T_MIN Expression {
            CALL(@1, @3, exprBinary(MIN));
        }
        | Expression T_MAX Expression {
            CALL(@1, @3, exprBinary(MAX));
        }
        | T_FORALL '(' Id ':' Type ')' {
            CALL(@1, @6, exprForAllBegin($3));
        } Expression {
            CALL(@1, @8, exprForAllEnd($3));
        } %prec T_FORALL
        | T_EXISTS '(' Id ':' Type ')' {
            CALL(@1, @6, exprExistsBegin($3));
        } Expression {
            CALL(@1, @8, exprExistsEnd($3));
        } %prec T_EXISTS
        | Assignment
	| T_AF Expression {
	    CALL(@1, @2, exprUnary(AF));
	}
        | T_AG Expression {
	    CALL(@1, @2, exprUnary(AG));
        }
	| T_EF Expression {
	    CALL(@1, @2, exprUnary(EF));
        }
        | T_AG T_MULT Expression {
	    CALL(@1, @2, exprUnary(AG_R));
        }
	| T_EF T_MULT Expression {
	    CALL(@1, @2, exprUnary(EF_R));
        }
	| T_EG Expression {
	    CALL(@1, @2, exprUnary(EG));
        }
        | T_AF2 PathExpression Expression {
	    CALL(@1, @3, exprBinary(AF2));
	}
        | T_AG2 PathExpression Expression {
	    CALL(@1, @3, exprBinary(AG2));
        }
        | T_EF2 PathExpression Expression {
	    CALL(@1, @3, exprBinary(EF2));
	}
        | T_EG2 PathExpression Expression {
	    CALL(@1, @3, exprBinary(EG2));
	}
	| Expression T_LEADSTO Expression {
	    CALL(@1, @3, exprBinary(LEADSTO));
        }
	| 'A' '[' Expression 'U' Expression ']' {
	    CALL(@1, @6, exprTernary(A_UNTIL, true));
        }
	| 'A' '[' Expression 'W' Expression ']' {
	    CALL(@1, @6, exprTernary(A_WEAKUNTIL, true));
        }
	| 'A' '{' Expression '}' '[' Expression 'U' Expression ']' {
	    CALL(@1, @7, exprTernary(A_UNTIL));
        }
	| 'A' '{' Expression '}' '[' Expression 'W' Expression ']' {
	    CALL(@1, @7, exprTernary(A_WEAKUNTIL));
        }
        ;

PathExpression:
	/* empty */ { ch->exprTrue(); }
        | '{' Expression '}';

Assignment:
        Expression AssignOp Expression { 
          CALL(@1, @3, exprAssignment($2));
        } %prec T_ASSIGNMENT;

AssignOp: 
        /* = += -= /= %= &= |= ^= <<= >>= */
          T_ASSIGNMENT { $$ = ASSIGN; }
        | T_ASSPLUS   { $$ = ASSPLUS; }
        | T_ASSMINUS  { $$ = ASSMINUS; }
        | T_ASSDIV    { $$ = ASSDIV; }
        | T_ASSMOD    { $$ = ASSMOD; }
        | T_ASSMULT   { $$ = ASSMULT; }
        | T_ASSAND    { $$ = ASSAND; }
        | T_ASSOR     { $$ = ASSOR; }
        | T_ASSXOR    { $$ = ASSXOR; }
        | T_ASSLSHIFT { $$ = ASSLSHIFT; }
        | T_ASSRSHIFT { $$ = ASSRSHIFT; }
        ;

UnaryOp: 
        /* - + ! */
        T_MINUS       { $$ = MINUS; }
        | T_PLUS      { $$ = PLUS; }
        | T_EXCLAM    { $$ = NOT; }
        ;

ArgList:
        /* The result is the number of expressions as parameters */
        /* empty */ { $$=0; }
        | Expression { 
            $$ = 1; 
        }
        | ArgList ',' Expression { 
            $$ = $1 + 1; 
        }
        ;

/***********************************************************************
 * Old XTA format grammar
 */

OldXTA: 
        OldDeclaration Instantiations System;


OldDeclaration:
        /* empty */
        | OldDeclaration OldVarDecl
        | OldDeclaration OldProcDecl
        ;

OldVarDecl:
        VariableDecl
        | T_OLDCONST { 
          CALL(@1, @1, typeInt(ParserBuilder::PREFIX_CONST));
        } OldConstDeclIdList ';' { 
          CALL(@1, @3, typePop());
        }
        | T_OLDCONST error ';';

OldConstDeclIdList:
        OldConstDeclId 
        | OldConstDeclIdList ',' OldConstDeclId
        ;

OldConstDeclId:
        NonTypeId {
          CALL(@1, @1, typeDuplicate());
        } ArrayDecl Initializer { 
          CALL(@1, @4, declVar($1, true));
        }
        ;

/*********************************************************************
 * Old Process declaration
 */
OldProcDecl:
        T_PROCESS Id OldProcParams '{' { 
          CALL(@1, @4, procBegin($2));
        }
        OldProcBody '}' { 
          CALL(@5, @6, procEnd());
        } 
        | T_PROCESS Id OldProcParams error '{' { 
          CALL(@1, @5, procBegin($2));
        }
        OldProcBody '}' { 
          CALL(@6, @7, procEnd());
        } 
        | T_PROCESS Id error '{' { 
          CALL(@1, @4, procBegin($2));
        }
        OldProcBody '}' { 
          CALL(@5, @6, procEnd());
        } 
        | T_PROCESS error '{' { 
          CALL(@1, @3, procBegin("_"));
        }
        OldProcBody '}' { 
          CALL(@4, @5, procEnd());
        } 
        | T_PROCESS Id '{' { 
          CALL(@1, @3, procBegin($2));
        }
        OldProcBody '}' { 
          CALL(@4, @5, procEnd());
        }
        ;

OldProcParams:
        '(' ')' 
        | '(' OldProcParamList ')' 
        | '(' error ')' 
        ;

OldProcParamList:
        OldProcParam { 
          CALL(@1, @1, typePop());
        }
        | OldProcConstParam 
        | OldProcParamList ';' OldProcParam { 
          CALL(@1, @3, typePop());
        }
        | OldProcParamList ';' OldProcConstParam
        ;

OldProcParam:
        Type {
            CALL(@1, @1, typeDuplicate());
        } NonTypeId ArrayDecl {
            CALL(@1, @4, declParameter($3, true));
        }
        | OldProcParam {
            CALL(@1, @1, typeDuplicate());
        } ',' NonTypeId ArrayDecl { 
            CALL(@1, @5, declParameter($4, true));
        } 
        ;

OldProcConstParam:
        T_OLDCONST {
            CALL(@1, @1, typeInt(ParserBuilder::PREFIX_CONST));
        } NonTypeId ArrayDecl {
            CALL(@3, @4, declParameter($3, false));
        }
        | OldProcConstParam ',' {
            CALL(@1, @1, typeInt(ParserBuilder::PREFIX_CONST));
        } NonTypeId ArrayDecl { 
            CALL(@4, @5, declParameter($4, false));
        } 
        ;

OldProcBody:
        OldVarDeclList OldStates LocFlags Init OldTransitions
        ;

OldVarDeclList:
        /* empty */
        | OldVarDeclList OldVarDecl
        ;

OldStates:
        T_STATE OldStateDeclList ';'
        | error ';'
        ;

OldStateDeclList:
        OldStateDecl
        | OldStateDeclList ',' OldStateDecl
        ;

OldStateDecl:
        NonTypeId { 
          CALL(@1, @1, exprTrue(); ch->procState($1, false));
        }
        | NonTypeId '{' OldInvariant '}' { 
          CALL(@1, @4, procState($1, true));
        }
        ;

OldInvariant:
        Expression
        | Expression error ',' {	  
        }
        | OldInvariant ',' Expression { 
          CALL(@1, @3, exprBinary(AND));
        }
        ;

OldTransitions:
        /* empty */
        | T_TRANS OldTransitionList ';'
        | T_TRANS error ';' 
        ;

OldTransitionList:
        OldTransition
        | OldTransitionList ',' OldTransitionOpt
        ;

OldTransition:
        NonTypeId T_ARROW NonTypeId '{' {
            CALL(@1, @3, procEdgeBegin($1, $3, true));
        } OldGuard Sync Assign '}' { 
            strcpy(rootTransId, $1);
            CALL(@1, @8, procEdgeEnd($1, $3));
        }
        ;


OldTransitionOpt:
        T_ARROW NonTypeId '{' {
            CALL(@1, @2, procEdgeBegin(rootTransId, $2, true));
        } OldGuard Sync Assign '}' { 
            CALL(@1, @7, procEdgeEnd(rootTransId, $2));
        }
        | OldTransition
        ;

OldGuard:
        /* empty */
        | T_GUARD OldGuardList ';' {
          CALL(@2, @2, procGuard());
        }
        | T_GUARD OldGuardList error ';' {
          CALL(@2, @3, procGuard());
        }
        ;

OldGuardList:
        Expression
        | OldGuardList ',' Expression { 
          CALL(@1, @3, exprBinary(AND));
        }
        ;

PropertyList:
          PropertyList2 Property;

PropertyList2:
        /* empty */
        | PropertyList2 Property '\n';

SimpleId:
	NonTypeId {
	    CALL(@1, @1, declParamId($1));
        };

SimpleIdList:
        SimpleId {
	    $$ = 1;
	}
        | SimpleIdList ',' SimpleId {
	    $$ = $1 + 1;
	};

PropertyExpr:
        Expression {
            CALL(@1, @1, property());
	}
        | T_CONTROL ':' Expression {
            CALL(@1, @3, exprUnary(CONTROL));
            CALL(@1, @3, property());
        }
        | T_CONTROL_T T_MULT '(' Expression ',' Expression ')' ':' Expression {
	    CALL(@1, @9, exprTernary(CONTROL_TOPT));
	    CALL(@1, @9, property());
	}
        | T_EF T_CONTROL ':' Expression {
	    CALL(@1, @4, exprUnary(EF_CONTROL));
	    CALL(@1, @4, property());
	};

Property:
	/* empty */
        | PropertyExpr OptStateQuery
        | T_INF SimpleIdList ':' PropertyExpr {
	    CALL(@1, @4, paramProperty($2, INF));
        }
        | T_SUP SimpleIdList ':' PropertyExpr {
	    CALL(@1, @4, paramProperty($2, SUP));
        };

OptStateQuery:
	/* empty */
	| T_INIT  {
	  CALL(@1, @1, ssQueryBegin());
	} BlockLocalDeclList T_TRANS StatementList T_RESULTSET ExprList ';' {
	  CALL(@1, @2, ssQueryEnd());
	};

%%

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

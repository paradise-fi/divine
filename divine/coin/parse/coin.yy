
%{
	/* CoIn-DVE parser */
#include <cstdio>
#include <cstring>

#include "coin_common.hh"

#define coin_system the_coin_system

%}

%union{
	char string[MAXLEN];
	label_t label;
}

%token T_SYSTEM "system"
%token T_PRIMITIVE "automaton"
%token T_COMPOSED "composite"
%token T_PROPERTY "property"
%token T_ARROW "->"
%token T_INIT "init"
%token T_ACCEPT "accept"
%token T_TRANS "trans"
%token T_STATE "state"
%token T_RESTRICT "restrictL"
%token T_ONLY "onlyL"
%token T_NOT "not"
%token <string>T_ID "identifier"

%start CoIn

%error-verbose
%locations

%type <label> Label
%%

CoIn 		:  AutList System Property
		;

AutList 	: Automaton
		| AutList Automaton
		;

Automaton 	: Primitive
		| Composed 
		;

Primitive 	: T_PRIMITIVE T_ID { coin_system->automaton_begin($2); }
		  Hierarchy '{' PrimBody '}' { coin_system->automaton_end(); }
		;

PrimBody 	: States Initial Transitions
		;

States 		: T_STATE StatesList ';'
		;

StatesList 	: T_ID { coin_system->add_state($1); }
		| StatesList ',' T_ID { coin_system->add_state($3); }
		;

Initial 	: T_INIT T_ID ';' { coin_system->set_init_state($2); }
		;

Transitions 	: /* epsilon */			
		| T_TRANS TransList ';'
		;

TransList 	: Trans
		| TransList ',' TransExt
		| TransList ',' Trans
		;

Trans 		: T_ID T_ARROW T_ID Label 
		  { coin_system->add_trans($1, $4, $3); }
		;

TransExt	: T_ARROW T_ID Label 
		  { coin_system->add_trans($3, $2); }
		;

Label		: '(' T_ID ',' T_ID ',' T_ID ')' 
		  { $$ = coin_system->mklabel_int($2, $4, $6); }
		| '(' T_ID ',' T_ID ',' '-' ')' 
		  { $$ = coin_system->mklabel_out($2, $4);  }
		| '(' '-' ',' T_ID ',' T_ID ')' 
		  { $$ = coin_system->mklabel_in($4, $6); }
		;

Composed 	: T_COMPOSED T_ID { coin_system->automaton_begin($2, false); }
		  '{' CompBody '}' { coin_system->automaton_end(); }
		;

CompBody 	: CompList ';' CompSpec
		| CompList ';'
		;

CompList 	: T_ID { coin_system->add_to_complist($1); }
		| CompList ',' T_ID { coin_system->add_to_complist($3); }
		;

CompSpec	: T_RESTRICT { coin_system->set_restrict(true); }
		  LabelList ';'
		| T_ONLY { coin_system->set_restrict(false); }
		  LabelList ';'
		;

LabelList	: Label	 { coin_system->add_to_restrict($1); }
		| LabelList ',' Label { coin_system->add_to_restrict($3); }

System		: T_SYSTEM T_ID ';' { coin_system->set_system($2); }
		;

Property	: /* epsilon */
		| T_PROPERTY { coin_system->prop_begin(); }
		  '{' PropBody '}' { coin_system->prop_end(); }
		;

PropBody	: States Initial Accepting PropTransitions
		;

Accepting	: T_ACCEPT { coin_system->accept_start(); } 
		  StatesList ';'
		;

PropTransitions : T_TRANS PropTransList ';'
		;

PropTransList	: PropTrans
		| PropTransList ',' PropTrans
		;

PropTrans	: T_ID T_ARROW T_ID { coin_system->prop_trans_begin($1,$3); }
		  '{' PropLabel '}' '{' Preconditions '}'
		;

PropLabel	: LabelList
		| '*'		{ coin_system->any_label(); }
                | '*' '-' LabelList { coin_system->any_label(); }
		;

Preconditions	: /* true */	/* satisified by any state */
		| PrecondList
		;
	
PrecondList	: Precond
		| PrecondList ',' Precond

Precond		: Label		{ coin_system->add_guard($1, true); }
		| T_NOT Label	{ coin_system->add_guard($2, false); }
		;
Hierarchy	: Hier
		| /* epsilon */	
		  { yyerror("hierarchy description expected"); YYERROR; }
		;

Hier		: '(' CompNameList ')'
		| '(' HList ')'
		;

CompNameList	: T_ID			{ coin_system->add_comp_name($1); }
		| CompNameList ',' T_ID	{ coin_system->add_comp_name($3); }
		;

HList		: Hier
		| HList ',' Hier
		;

%%


  void yyerror (const char * s)
  {
    fprintf(stderr,
        "%d:%d-%d:%d %s\n", yylloc.first_line, yylloc.first_column+1,
	yylloc.last_line, yylloc.last_column, s);
    exit(5);
  }
 

#undef coin_system

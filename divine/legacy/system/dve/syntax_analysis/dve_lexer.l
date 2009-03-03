%option noyywrap
%option nodefault
%{
/* error space 35500-35599 */
#include <string.h>
#include <iostream>
#include "system/dve/syntax_analysis/dve_commonparse.hh"
#include "common/error.hh"
#include "system/dve/syntax_analysis/dve_keywords.cci"

/* Necessary to get the definition of EOF: */
#include <cstdio>


using namespace divine;
using std::ostream;

extern dve_position_t dve_yylloc;

//#define YY_DECL int lexer_flex(void)

/**
 * Location tracking inside scanner, taken from 
 * http://www.lrde.epita.fr/people/akim/compil/gnuprog2/Advanced-Use-of-Flex.html
 */

#define YY_USER_ACTION  dve_yylloc.last_column+=yyleng; 
//define YY_FATAL_ERROR(msg) { throw TypeException(msg); }
//moje: it had to be removed - we do not use throw
#define YY_FATAL_ERROR(msg) { gerr << msg << thr(); }
%}

alpha	[a-zA-Z_]
num	[0-9]+
idchr	[a-zA-Z0-9_$#]

%x comment

%%

%{
  /* At each yylex invocation, mark the current position as the
     start of the next token.  */
  dve_yylloc.step();
%}

<comment>{
  \n           { dve_yylloc.lines(yyleng); dve_yylloc.step(); }
  "*/"         { BEGIN(INITIAL); }
  "//"[^\n]*   /* Single line comments take precedence over multilines */;
  <<EOF>>      { dve_yyerror("Unclosed comment."); }
  [^*/\n]*     /* ignore anything thats not a '*' or '/' to increase speed */;
  .            /* ignore (multiline comments)*/
}

"\\"[\t ]*"\n"  { /* Use \ as continuation character */ 
                  dve_yylloc.lines(1); 
                  dve_yylloc.step();
                } 

"//"[^\n]*            /* ignore (singleline comment)*/;

[ \t]+		{ dve_yylloc.step(); }

"/*"            { BEGIN(comment); }

\n+		{
                  dve_yylloc.lines(yyleng); 
 		  dve_yylloc.step();
/*		  if (syntax == SYNTAX_PROPERTY)
		    return '\n';*/  /* to nenastane - to neparsuju */
                }

"."		{ return '.'; }
","		{ return ','; }
";"		{ return ';'; }
":"             { return ':'; }
"{"		{ return '{'; }
"}"		{ return '}'; }
"["		{ return '['; }
"]"		{ return ']'; }
"("             { return '('; }
")"             { return ')'; }
"?"		{ return '?'; }
"!"		{ return T_EXCLAM; }
"~"		{ return T_TILDE; }

"->"		{ return T_ARROW; }
"=>"		{ return T_WIDE_ARROW; }
"="		{ return T_ASSIGNMENT; }

"+"		{ return T_PLUS; }
"-"		{ return T_MINUS; }
"*"		{ return T_MULT; }
"/"		{ return T_DIV; }
"%"             { return T_MOD; }
"|"             { return T_OR; }
"&"             { return T_AND; }
"^"             { return T_XOR; }
"<<"            { return T_LSHIFT; }
">>"            { return T_RSHIFT; }
"||"            { return T_BOOL_OR; }
"&&"            { return T_BOOL_AND; }

"<="		{ return T_LEQ; }
"<"		{ return T_LT; }
">="		{ return T_GEQ; }
">"		{ return T_GT; }
"=="		{ return T_EQ; }
"!="		{ return T_NEQ; }

"++"            { return T_INCREMENT; }
"--"            { return T_DECREMENT; }

{alpha}{idchr}* {
/*  		if (strlen(decl_text) >= MaxIdLen ) */
/*  			declError << "Identifier too long. Only " << MaxIdLen  */
/*  				 << " characters are significant.\n"; */
		  const Keyword *keyword
		    = Keywords::in_word_set(yytext, strlen(yytext));
		  //cerr << "Hello: " << yytext << endl;
		  if (keyword != NULL /*&& (syntax & keyword->syntax)*/) {
		    //cerr << "je to klicove slovo" << endl;
		    return keyword->token;	  
                  /* obecne typy nahrazeny klicovymi slovy */
/*	          } else if (Table.isType(yytext)) {
		    strncpy(yylval.string, yytext, MAXLEN);
		    yylval.string[MAXLEN - 1] = '\0'; //if we uncomment add reg!
		    return T_TYPENAME;*/
                  } else {
		    //cerr << "neni to klicove slovo" << endl;
		    /* neaktivni - nerozlisuje verze syntaxe */
		    /*if (keyword != NULL && !(syntax & keyword->syntax)) {
		      yylval.string[0] = '_'; //if we uncomment add registering!
		      strncpy(yylval.string + 1, yytext, MAXLEN - 1);
		      yylval.string[MAXLEN - 1] = '\0';		      
		    } else {*/
		      strncpy(dve_yylval.string, yytext, MAXLEN);
		      dve_yylval.string[MAXLEN - 1] = '\0';
		    /*}*/
		    return T_ID;
		  }
                }

{num}		{ 
                  dve_yylval.number = atoi(yytext); 
                  return T_NAT; 
                }

.               { 
                  gerr << "Unknown symbol '" << yytext << "'" << thr(35500); 
                  return 1;  /* unreachable */
                }

<<EOF>>		{ return 0; }

%%

ostream &operator <<(ostream &out, const YYLTYPE &l) 
{
  if (l.first_line != l.last_line)                      
    out << l.first_line << "." << l.first_column << "-" 
	<< l.last_line << "." << (l.last_column-1);
  else if (l.first_column < l.last_column-1)
    out << l.first_line << "." << l.first_column << "-" << (l.last_column-1);
  else                                                          
    out << l.first_line << "." << l.first_column;
  //  out << " (" << l.first_char << "-" << l.last_char << ")";
  return out;
};

int yywrap() {
  return 1;
}


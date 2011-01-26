%{
	/* CoIn-DVE lexer */
#define yywrap coin_yywrap
#define yylloc coin_yylloc
#define yylval coin_yylval

#include <cstdio>
#include <cstring>

#include "coin_common.hh"

#include "coin.tab.hh"

void yylloc_step()
{
  yylloc.first_line = yylloc.last_line;
  yylloc.first_column = yylloc.last_column;
}

void yylloc_line(int num)
{
  yylloc.first_column = yylloc.last_column = 0;
  yylloc.first_line = yylloc.last_line += num;
}

#define YY_USER_ACTION yylloc.last_column += yyleng;

%}

VALID	[A-Za-z0-9_']

%x COMMENT

%%

%{
  yylloc_step();
%}

"/*"		{ BEGIN(COMMENT); }

<COMMENT>{
\n+		{ yylloc_line(yyleng); }
"*/"		{ BEGIN(INITIAL); }
[^*/\n]+	/* eat comments (faster?) */
<<EOF>>		{ fprintf (stderr, "unclosed comment"); exit(1); }
.		/* eat comments */
}

"//"[^\n]*	/* eat one-line comments */

primitive|automaton	{ return T_PRIMITIVE; }

composed|composite	{ return T_COMPOSED; }

state		{ return T_STATE; }

init		{ return T_INIT; }

accept		{ return T_ACCEPT; }

trans		{ return T_TRANS; }

system		{ return T_SYSTEM; }

property	{ return T_PROPERTY; }

"->"		{ return T_ARROW; }

restrict|restrictL	{ return T_RESTRICT; }

only|onlyL		{ return T_ONLY; }

"!"|not		{ return T_NOT; }

{VALID}+	{ strncpy(yylval.string, yytext, MAXLEN);
		  yylval.string[MAXLEN - 1] = '\0';
  		  return T_ID; }	

[ \t]+		{ yylloc_step(); }

\n+		{ yylloc_line(yyleng); }

[(){};,-] 	{ return yytext[0]; }

"*"		{ return yytext[0]; }

.		{ fprintf (stderr, "undefined character '%c' at %d:%d\n", yytext[0], yylloc.last_line, yylloc.last_column);
		  exit(1); }

%%

int yywrap()
{
  return 1;
}

#undef yywrap

YACC = bison
#LEX = flex
LEX = flex++
PERF = gperf
PERFFLAGS = -C -E -t -L C++ -c
IPATH = .

# On some Linux platforms you may have a new version of lex that
# requires a command line option for backward compatibility:
#LEXFLAGS = -l
LEXFLAGS =

all: dve_grammar.cc dve_expr_grammar.cc dve_trans_grammar.cc dve_proc_grammar.cc $(IPATH)/dve_gramsymb.hh dve_lexer.cc dve_keywords.cci

dve_keywords.cci: dve_keywords.gperf
	$(PERF) $(PERFFLAGS) -Z Keywords dve_keywords.gperf >dve_keywords.cci
	sed -i -e 's,^#line.*,,' $(@)

dve_grammar.cc $(IPATH)/dve_gramsymb.hh: dve_grammar.yy
	$(YACC) -p dve_yy -v -d dve_grammar.yy; mv dve_grammar.tab.cc dve_grammar.cc; \
	mv dve_grammar.tab.hh $(IPATH)/dve_gramsymb.hh
	sed -i -e 's,^#line.*,,' $(@)
dve_expr_grammar.cc: dve_grammar.yy
	sed 's/%start DVE/%start ExpressionStandalone/' <dve_grammar.yy | sed 's/init_parsing/expr_init_parsing/g' >dve_expr_grammar.yy
	$(YACC) -p dve_ee -d dve_expr_grammar.yy 2>/dev/null; sed 's/^YYLTYPE yylloc/extern YYLTYPE dve_yylloc/' <dve_expr_grammar.tab.cc | sed 's/^YYSTYPE yylval/extern YYSTYPE dve_yylval/' | sed 's/#define *yylval .*/#define yylval dve_yylval/'| sed 's/#define *yylloc .*/#define yylloc dve_yylloc/' >dve_expr_grammar.cc;
	rm dve_expr_grammar.tab.cc
	rm dve_expr_grammar.tab.hh
	rm dve_expr_grammar.yy
	sed -i -e 's,^#line.*,,' $(@)
dve_trans_grammar.cc: dve_grammar.yy
	sed 's/%start DVE/%start TransitionStandalone/' <dve_grammar.yy | sed 's/init_parsing/trans_init_parsing/g' >dve_trans_grammar.yy
	$(YACC) -p dve_tt -d dve_trans_grammar.yy 2>/dev/null; sed 's/^YYLTYPE yylloc/extern YYLTYPE dve_yylloc/' <dve_trans_grammar.tab.cc | sed 's/^YYSTYPE yylval/extern YYSTYPE dve_yylval/' | sed 's/#define *yylval .*/#define yylval dve_yylval/'| sed 's/#define *yylloc .*/#define yylloc dve_yylloc/' >dve_trans_grammar.cc;
	rm dve_trans_grammar.tab.cc
	rm dve_trans_grammar.tab.hh
	rm dve_trans_grammar.yy
	sed -i -e 's,^#line.*,,' $(@)
dve_proc_grammar.cc: dve_grammar.yy
	sed 's/%start DVE/%start ProcDeclStandalone/' <dve_grammar.yy | sed 's/init_parsing/proc_init_parsing/g' >dve_proc_grammar.yy
	$(YACC) -p dve_pp -d dve_proc_grammar.yy 2>/dev/null; sed 's/^YYLTYPE yylloc/extern YYLTYPE dve_yylloc/' <dve_proc_grammar.tab.cc | sed 's/^YYSTYPE yylval/extern YYSTYPE dve_yylval/' | sed 's/#define *yylval .*/#define yylval dve_yylval/'| sed 's/#define *yylloc .*/#define yylloc dve_yylloc/' >dve_proc_grammar.cc;
	rm dve_proc_grammar.tab.cc
	rm dve_proc_grammar.tab.hh
	rm dve_proc_grammar.yy
	sed -i -e 's,^#line.*,,' $(@)
dve_lexer.cc: dve_lexer.l
	$(LEX) $(LEXFLAGS) -o dve_lexer.cc dve_lexer.l;
	sed -i 's/#include <FlexLexer.h>/#include "system\/dve\/syntax_analysis\/dve_flex_lexer.hh"/' dve_lexer.cc
	sed -i -e 's,^#line.*,,' $(@)
	cp /usr/include/FlexLexer.h $(IPATH)/dve_flex_lexer.hh

clean:
	rm dve_keywords.cci dve_grammar.cc $(IPATH)/dve_gramsymb.hh \
	   dve_expr_grammar.cc dve_trans_grammar.cc dve_proc_grammar.cc\
	   dve_lexer.cc

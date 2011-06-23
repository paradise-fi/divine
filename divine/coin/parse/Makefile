PREFIX=coin_yy

YACC=bison
LEX=flex
RM=rm -f
CXX=g++
CXXFLAGS=-g -pedantic
YFLAGS=-d --name-prefix=$(PREFIX)
LFLAGS=--prefix=$(PREFIX)

YINPUT=coin.yy
LINPUT=coin.l
YBASE=$(YINPUT:.yy=.tab)

PARSER_SRC=$(YBASE).cc
PARSER_H=$(YBASE).hh
LEXER_SRC=lex.$(LINPUT:.l=.cc)

GENERATED=$(PARSER_SRC) $(PARSER_H) $(LEXER_SRC)

all: $(GENERATED)

$(PARSER_SRC) $(PARSER_H): $(YINPUT)
	$(YACC) $(YFLAGS) $<

$(LEXER_SRC): $(LINPUT) coin_common.hh $(PARSER_H)
	$(LEX) $(LFLAGS) -o $@ $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(GENERATED) *.o





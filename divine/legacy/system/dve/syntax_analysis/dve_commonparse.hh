/*!\file
 * Auxiliary header file used in parser. You should not need to use it
 * explicitly during an implementation of program based on this library
 *
 * \author Pavel Simecek
 */

//THIS IS ENVELOPE FOR gramsymb.hh
//!!!DO NEVER INCLUDE ONLY gramsymb.hh

#ifndef _DVE_COMMONPARSE_HH_
#define _DVE_COMMONPARSE_HH_

#define MAXLEN 64

//!Structure for storing of position in a source code
/*!This structure is used in an implementation of parser of DVE files
 * You should not need to use it during an implementation of program
 * based on this library*/
struct dve_position_t
{
 int first_line, first_column, last_line, last_column;
 dve_position_t() { reset(); }
 dve_position_t(const dve_position_t &loc): 
   first_line(loc.first_line), first_column(loc.first_column),
   last_line(loc.last_line), last_column(loc.last_column) {};
 ~dve_position_t(){ };
 void reset() {
  first_column = first_line = 1; 
  last_column = last_line = 1; 
 };
 void lines(int num) {
  last_column = 1;
  last_line += num;
 };
 void step() {
  first_column = last_column;       
  first_line = last_line;
 };
};

#define YYLTYPE dve_position_t

//!Method called by Bison's parser in case of unrecorvable syntax error
//! in DVE file
void dve_yyerror(const char * msg);
//!Bison's parser of DVE files
int dve_yyparse();
//!Method called by Bison's parser in case of unrecorvable syntax error
//! in expression (of DVE syntax)
void dve_eeerror(const char * msg);
//!Bison's parser of DVE expressions
int dve_eeparse();
//!Method called by Bison's parser in case of unrecorvable syntax error
//! in transition (of DVE syntax)
void dve_tterror(const char * msg);
//!Bison's parser of DVE transactions
int dve_ttparse();
//!Method called by Bison's parser in case of unrecorvable syntax error
//! in process (of DVE syntax)
void dve_pperror(const char * msg);
//!Bison's parser of DVE processes
int dve_ppparse();

#include "system/dve/syntax_analysis/dve_gramsymb.hh"

#endif

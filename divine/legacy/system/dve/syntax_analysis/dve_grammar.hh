/*!\file
 * This file only contains function *init_parsing() which are further
 * used in system_t::read(), process_t::read(), transition_t::read()
 * and expression_t::read(). */
#ifndef _GRAMMAR_HH_
#define _GRAMMAR_HH_

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include "system/dve/syntax_analysis/dve_commonparse.hh"
#include "system/dve/syntax_analysis/dve_parser.hh"

namespace divine //We want Doxygen not to see namespace `dve'
{
#endif //DOXYGEN_PROCESSING

//!Initializes a Bison's parser of DVE files before parsing
void dve_init_parsing(dve_parser_t * const, error_vector_t *, std::istream & mystream);
//!Initializes a Bison's parser of DVE expressions before parsing
void dve_expr_init_parsing(dve_parser_t * const, error_vector_t *, std::istream & mystream);
//!Initializes a Bison's parser of DVE transitions before parsing
void dve_trans_init_parsing(dve_parser_t * const, error_vector_t *, std::istream & mystream);
//!Initializes a Bison's parser of DVE processes before parsing
void dve_proc_init_parsing(dve_parser_t * const, error_vector_t *, std::istream & mystream);

#ifndef DOXYGEN_PROCESSING
};
#endif //DOXYGEN_PROCESSING

#endif

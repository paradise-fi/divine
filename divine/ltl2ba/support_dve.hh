/*
*  Transformation LTL formulae to Buchi automata
*
*    - colection of helping functions
*      - reading LTL formulae from text file
*      - writing Buchi automaton to DiVinE source file
*  
*  Milan Jaros - xjaros1@fi.muni.cz
*
*/

#ifndef DIVINE_LTL_SUP_DVE
#define DIVINE_LTL_SUP_DVE

//#define CC_ONLY_OPT
/* compile only functions for writing automaton in class 'opt_graph' */

#ifndef DOXYGEN_PROCESSING

#include <iostream>
#include <fstream>
#include <map>
#include <string>

#include "formul.hh"
#include "ltl.hh"
#include "KS_BA_graph.hh"
#include "BA_graph.hh"
#include "opt_buchi.hh"

#ifndef CC_ONLY_OPT
#include "ltl_graph.hh"
#endif //CCONLY_OPT

//#include "deb/deb.hh"

namespace divine {
#endif //DOXYGEN_PROCESSING

/* Reading of LTL formulae from text file:
L_ltl = list of (unprocessed) LTL formulae
L_prop = list of at. propositions in LTL formulae
*/
bool read_ltl_file(std::istream& fr, list<LTL_parse_t>& L_ltl,
	list<map<string, string> >& L_prop);

/* setings of at. propositions */
void reg_ltl_at_props(map<string, string> *p_AP);

/* 'file_name[.dve]' is DiVinE source file. Automaton will writen to new 
source file "file_name.prop'n'.dve" */
#ifndef CC_ONLY_OPT
/* writing GBA */
void output_to_file(const ltl_graph_t& G, string file_name, int n = 0);
#endif

void output_to_file(const BA_graph_t& G, string file_name, int n = 0);

#ifndef CC_ONLY_OPT
/* writing GBA */
void output(const ltl_graph_t& G, std::ostream& fw);
#endif
void output(const BA_graph_t& G, std::ostream& fw);


#ifndef DOXYGEN_PROCESSING
} //END of namespace DVE
//#include "deb/undeb.hh"
#endif //DOXYGEN_PROCESSING

#endif

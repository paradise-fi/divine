#include <string>
#include <sstream>
#include "system/dve/dve_prob_transition.hh"
#include "system/dve/dve_transition.hh"
#include "system/dve/syntax_analysis/dve_grammar.hh"

#include <wibble/string.h>

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

using std::string;

dve_parser_t dve_prob_transition_t::prob_trans_parser(*ptrans_terr, (dve_prob_transition_t *)(0));

std::string dve_prob_transition_t::to_string() const
{
 std::ostringstream ostr;
 write(ostr);
 return ostr.str();
}

void dve_prob_transition_t::write(std::ostream & ostr) const
{
 string aux="";
 size_int_t minus=0;
 string str;
 if (!trans.size())
   (*transition_t::ptrans_terr) << "Unexpected error: No content of prob. transition" << thr();
 dve_transition_t * first_trans=dynamic_cast<dve_transition_t*>(trans[0]);
 str = string(first_trans->get_state1_name()) + " => { ";
 
 for (size_int_t i=0; i!=trans.size(); i++)
  {
   dve_transition_t * dve_trans =
     dynamic_cast<dve_transition_t*>(trans[i]);
   std::string prob_weight_i = wibble::str::fmt(prob_weights[i]);
   
   aux = std::string(dve_trans->get_state2_name()) + ":"
         + prob_weight_i
         + ((i==(trans.size()-1)) ? " }" : ", ");
   if (aux.size()+str.size()-minus>80) { str += "\n     "; minus+=80; }

   str += aux;
  }
  
 ostr << str;
 
}

int dve_prob_transition_t::read(std::istream & istr, size_int_t process_gid)
{
 for (size_int_t i=0; i!=trans.size(); ++i)
   delete trans[i];
 trans.clear();
 prob_weights.clear();
 weight_sum = 0;
 dve_trans_init_parsing(&prob_trans_parser, transition_t::ptrans_terr, istr);
 prob_trans_parser.set_dve_prob_transition_to_fill(this);
 prob_trans_parser.set_current_process(process_gid);
 try
  { dve_ttparse(); }
 catch (ERR_throw_t & err)
  { return err.id; } //returns non-zero value
 return 0; //returns zero in case of successful parsing
}


int dve_prob_transition_t::from_string(std::string & trans_str,
                                       const size_int_t process_gid)
{
 std::istringstream str_stream(trans_str);
 return read(str_stream);
}


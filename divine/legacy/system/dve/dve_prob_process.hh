/*!\file
 * The main contribution of this file is the abstact interface dve_prob_process_t.
 */
#ifndef DIVINE_DVE_PROB_PROCESS_HH
#define DIVINE_DVE_PROB_PROCESS_HH

#ifndef DOXYGEN_PROCESSING

#include "system/system.hh"
#include "system/dve/syntax_analysis/dve_parser.hh"
#include "system/prob_process.hh"
#include "system/dve/dve_process.hh"
#include "system/dve/dve_prob_transition.hh"

namespace divine { //We want Doxygen not to see namespace `divine'

#endif //DOXYGEN_PROCESSING

//predeclarations of classes:
class prob_process_t;
class dve_prob_transition_t;
class dve_process_t;
class dve_parser_t;

//!Class representing a DVE process in probabilistic system
/*!This class implements the abstract interface prob_process_t.
 */
class dve_prob_process_t: public prob_process_t, public dve_process_t
{
 private:
 static dve_parser_t prob_proc_parser;   
 array_t<dve_prob_transition_t*> prob_transitions;

 public:
 //!A constructor
 dve_prob_process_t() {}
 //!A constructor
 dve_prob_process_t(prob_system_t * const system): process_t(system),
                               prob_process_t(system), dve_process_t(system) {}
 //!A destructor
 virtual ~dve_prob_process_t();
 //!Implements prob_process_t::add_prob_transition() for DVE probabilistics process
 virtual void add_prob_transition(prob_transition_t * const prob_trans);
 //!Implements prob_process_t::remove_prob_transition() for DVE probabilistics process
 virtual void remove_prob_transition(const size_int_t transition_index);
 //!Implements prob_process_t::get_prob_transition() for DVE probabilistics process
 virtual prob_transition_t * get_prob_transition
                                     (const size_int_t prob_trans_lid);
 //!Implements prob_process_t::get_prob_transition() for DVE probabilistics process
 virtual const prob_transition_t * get_prob_transition
                                (const size_int_t prob_trans_lid) const;

 //!Implements prob_process_t::get_prob_trans_count() for DVE probabilistics process
 virtual size_int_t get_prob_trans_count() const
  { return prob_transitions.size(); }
  
 //!Implements process_t::to_string() for DVE probabilistics process
 virtual std::string to_string() const;
 //!Implements process_t::write() for DVE probabilistics process
 virtual void write(std::ostream & ostr) const;
 //!Implements process_t::from_string() for DVE probabilistics process
 virtual int from_string(std::string & proc_str);
 //!Implements process_t::read() for DVE probabilistics process
 virtual int read(std::istream & istr);
};


#ifndef DOXYGEN_PROCESSING  
} //END of namespace divine

#endif //DOXYGEN_PROCESSING

#endif





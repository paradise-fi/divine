/*!\file
 * The main contribution of this file is the class dve_prob_explicit_system_t
 */
#ifndef DIVINE_DVE_PROB_EXPLICIT_SYSTEM_HH
#define DIVINE_DVE_PROB_EXPLICIT_SYSTEM_HH

#ifndef DOXYGEN_PROCESSING
#include "system/dve/dve_explicit_system.hh"
#include "system/dve/dve_prob_system.hh"
#include "system/prob_explicit_system.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING


//!Class in DVE system interpretation
/*!This class implements the abstract interface explicit_system_t.
 *
 * It is a child of dve_system_t - thus it also contains the representation
 * of DVE \sys.
 *
 * DVE system interpretation in this case comprises state generation,
 * enabled transitions generation and expression evaluation.
 * 
 * This implementation of the abstract interface implements full set of
 * its methods. Furthermore
 * It takes over the system of expression evaluation from system_t. Only
 * for evaluating varibles, fields and state identifiers there are defined
 * special functions, which return their value accoring a state of system
 * (given by a piece of a memory).
 *
 * Furthermore it provides the set of methods, which are purely DVE system
 * specific.
 */
class dve_prob_explicit_system_t : public dve_explicit_system_t, public prob_explicit_system_t, public dve_prob_system_t
{
 private:
  succ_container_t only_succs;
  prob_succ_container_t aux_prob_succs;
 public:
 ///// VIRTUAL INTERFACE METHODS: /////
  virtual int get_succs(state_t state, prob_succ_container_t & succs, enabled_trans_container_t & etc);

  virtual int get_succs(state_t state, prob_succ_container_t & succs)
  {
    int result=get_succs(state,succs,*aux_enabled_trans);
    return result;
  }

  virtual int get_succs_ordered_by_prob_and_property_trans(state_t state, prob_succ_container_t & succs);
  
  virtual slong_int_t read(std::istream & ins) 
  { return dve_prob_system_t::read(ins); }
  
  virtual slong_int_t read(const char * const filename)
  { return dve_explicit_system_t::read(filename); }


 //!A constructor
 /*!\param evect = \evector used for reporting of error messages*/
 dve_prob_explicit_system_t(error_vector_t & evect = gerr): explicit_system_t(evect), dve_explicit_system_t(evect), prob_explicit_system_t(evect), only_succs(), aux_prob_succs() {}
 //!A destructor
 virtual ~dve_prob_explicit_system_t() {};
 
}; //END of class dve_prob_explicit_system_t



#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif

/*!\file
 * The main contribution of this file is the class dve_prob_system_t
 */
#ifndef DIVINE_DVE_PROB_SYSTEM_HH
#define DIVINE_DVE_PROB_SYSTEM_HH

#ifndef DOXYGEN_PROCESSING
#include <fstream>
#include "system/dve/dve_system.hh"
#include "system/prob_system.hh"
#include "common/error.hh"

//The main DiVinE namespace - we do not want Doxygen to see it
namespace divine {
#endif //DOXYGEN_PROCESSING

class dve_prob_system_t: public virtual prob_system_t,
                         public virtual dve_system_t
 {
  public:
  //!A constructor.
  /*!\param evect =
   * the <b>error vector</b>, that will be used by created instance of system_t
   */
  dve_prob_system_t(error_vector_t & evect = gerr): system_t(evect),
                                                    prob_system_t(evect),
                                                    dve_system_t(evect) {}
  
  //!A destructor.
  virtual ~dve_prob_system_t() {}
  
  virtual slong_int_t read(std::istream & ins)
  {
   slong_int_t result = dve_system_t::read(ins);
   if (result==0) prob_system_t::consolidate();
   return result;
  }
  
  
//  //Reimplementation for probabilistic systems:
//  virtual bool write(const char * const filename);
//  virtual void write(std::ostream & outs = std::cout);
//  virtual std::string to_string();
  
  
 };

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#endif //DOXYGEN_PROCESSING

#endif

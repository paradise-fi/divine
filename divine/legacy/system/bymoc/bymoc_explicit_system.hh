/*!\file
 * The main contribution of this file is the class bymoc_explicit_system_t
 */
#ifndef DIVINE_BYMOC_EXPLICIT_SYSTEM_HH
#define DIVINE_BYMOC_EXPLICIT_SYSTEM_HH

#ifndef DOXYGEN_PROCESSING
#include "system/explicit_system.hh"
#include "system/bymoc/bymoc_system.hh"
#include "system/state.hh"
#include "system/bymoc/bymoc_system_trans.hh"
#include "common/types.hh"
#include "common/deb.hh"
#ifdef count
 #undef count
#endif
#ifdef max
 #undef max
#endif
#ifdef min
 #undef min
#endif
#ifdef PACKED
 #undef PACKED
#endif

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

class succ_container_t_;
class enabled_trans_container_t;
//class bymoc_system_trans_t;
//class bymoc_enabled_trans_t;


//!Class serving for evaluation of possible transitions
//! (of a system given by bytecode
//! source) by the way of explicit state creating.
/*!bymoc_explicit_system_t is the immediate descendant of a class system_t.
 *
 * It takes over the system of expression evaluation from system_t. Only
 * for evaluating varibles, fields and state identifiers there are defined
 * special functions, which return their value accoring a state of system
 * (given by a piece of a memory).
 *
 *
 */

class bymoc_explicit_system_t : public explicit_system_t, public bymoc_system_t
{
 //PRIVATE PART:
 private:

 process_decomposition_t *property_decomposition;
    
 //PUBLIC PART:
 public:
 
 ///// VIRTUAL INTERFACE METHODS: /////
 
 //!A constructor
 /*!\param evect = \evector used for reporting of error messages*/
 bymoc_explicit_system_t(error_vector_t & evect = gerr);
 //!A destructor
 virtual ~bymoc_explicit_system_t();//!<A destructor.
 
 /*! @name Obligatory part of abstact interface
    These methods have to implemented in each implementation of
    explicit_system_t  */
 //!Implements explicit_system_t::is_erroneous() in BYMOC \sys, but
 //! see also implementation specific notes below
 /*!It constantly returns true - till now virtual machine does not
  * support any constrol of error states (created e. g. by division
  * by zero)*/
 virtual bool is_erroneous(state_t state);
 //!Implements explicit_system_t::is_accepting() in BYMOC \sys
 virtual bool is_accepting(state_t state, size_int_t acc_group=0, size_int_t pair_member=1);

 //!Implements explicit_system_t::violates_assertion() in BYMOC
 /*!Currently it only returns false, because assertions are not supported by
  * BYMOC */
  virtual bool violates_assertion(const state_t state) const 
  { if (state.ptr) return false;
    else return false;
  }

 //!Implements explicit_system_t::violated_assertion_count() in BYMOC
 /*!Currently it only returns 0, because assertions are not supported by
  * BYMOC */
 virtual size_int_t violated_assertion_count(const state_t state) const
 {  if (state.ptr) return 0;
    else return 0;
 }
 
 //!Implements explicit_system_t::violated_assertion_string() in BYMOC
 /*!Currently it only returns empty string, because assertions are not
  * supported by BYMOC */
 virtual std::string violated_assertion_string(const state_t state,
                                               const size_int_t index) const
  { if ((state.ptr) || index) return std::string(""); 
    else return std::string(""); 
  }
                                                   
 
 //!Implements explicit_system_t::get_preperty_type()
 virtual property_type_t get_property_type();


 //!Implements explicit_system_t::print_state() in BYMOC \sys, but
 //! see also implementation specific notes below
 /*!This methods always returns 10000. No better estimation is implemented.*/
 virtual size_int_t get_preallocation_count() const;
 //!Implements explicit_system_t::print_state() in BYMOC \sys, but
 //! see also implementation specific notes below
 virtual void print_state(state_t state, std::ostream & outs = std::cout);
 //!Implements explicit_system_t::get_initial_state() in BYMOC \sys
 virtual state_t get_initial_state();
 //!Implements explicit_system_t::get_succs() in BYMOC \sys
 virtual int get_succs(state_t state, succ_container_t & succs);
 //!Implements explicit_system_t::get_ith_succ() in BYMOC \sys
 virtual int get_ith_succ(state_t state, const int i, state_t & succ);
 /*@}*/

 process_decomposition_t * get_property_decomposition();
    
///// BYMOC EXPLICIT SYSTEM CANNOT WORK WITH SYSTEM TRANSITITONS /////
/*! @name Methods working with system transitions and enabled transitions
   These methods are not implemented and can_system_transitions() returns false
  @{*/

 //!Not imlemented in BYMOC \sys - throws error message
 virtual int get_succs(state_t state, succ_container_t & succs,
               enabled_trans_container_t & etc);
 //!Not imlemented in BYMOC \sys - throws error message
 virtual int get_enabled_trans(const state_t state,
                       enabled_trans_container_t & enb_trans);
 //!Not imlemented in BYMOC \sys - throws error message
 virtual int get_enabled_trans_count(const state_t state, size_int_t & count);
 //!Not imlemented in BYMOC \sys - throws error message
 virtual int get_enabled_ith_trans(const state_t state,
                                 const size_int_t i,
                                 enabled_trans_t & enb_trans);
 //!Not imlemented in BYMOC \sys - throws error message
 virtual bool get_enabled_trans_succ
   (const state_t state, const enabled_trans_t & enabled,
    state_t & new_state);
 //!Not imlemented in BYMOC \sys - throws error message
 virtual bool get_enabled_trans_succs
   (const state_t state, succ_container_t & succs,
    const enabled_trans_container_t & enabled_trans);
 //!Not imlemented in BYMOC \sys - throws error message
 virtual enabled_trans_t * new_enabled_trans() const;
 /*@}*/
 
///// BYMOC EXPLICIT SYSTEM CANNOT EVALUATE EXPRESSIONS /////
 /*! @name Methods for expression evaluation
   These methods are not implemented and can_evaluate_expressions() returns
   false
  @{*/
 //!Not imlemented in BYMOC \sys - throws error message
 virtual bool eval_expr(const expression_t * const expr,
                        const state_t state,
                        data_t & data) const;
 /*@}*/
 
}; //END of class bymoc_explicit_system_t



#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif

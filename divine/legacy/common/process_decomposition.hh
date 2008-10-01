/*!\file process_decomposition.hh
 * Process graph decomposition into SCCs including types */
#ifndef _DIVINE_PROCESS_DECOMPOSITION_HH_
#define _DIVINE_PROCESS_DECOMPOSITION_HH_

#include <cstddef>

#ifndef DOXYGEN_PROCESSING
namespace divine {
#endif
  class explicit_system_t;
  class state_t;  
  
  /*! This class is used to decompose a graph of a single process specified
   *  in a dve file into SCCs. The decomposition is not accesible directly
   *  but with member functions returning for a given state SCC id and
   *  type. */
  class process_decomposition_t 
  {
  public:
    /*! Performs the decomposition of a process with a given (global) id. */
    virtual void parse_process(std::size_t) = 0;

    /*! Returns id of an SCC that the given local state of the process belongs to. */
    virtual int get_process_scc_id(state_t&) = 0;

    /*! Returns type of an SCC that the given local state belongs to. Returned
      values: 0 means nonaccepting component, 1 means partially accepting
      component, and 2 means fully accepting component. */
    virtual int get_process_scc_type(state_t&) = 0;
    
    /*! Returns type of the given SCC, where 0 means nonaccepting component, 1
      means partially accepting component, and 2 means fully accepting
      component. */
    virtual int get_scc_type(int) = 0;
    
    /*! Returns the number of SCCs in the decomposition.*/
    virtual int get_scc_count() = 0;

    /*! Returns whether the process has a weak graph. */
    virtual bool is_weak() = 0;

    virtual ~process_decomposition_t() {};
};


#ifndef DOXYGEN_PROCESSING
}
#endif
#endif








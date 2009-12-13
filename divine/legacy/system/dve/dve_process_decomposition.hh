/*!\file dve_process_decomposition.hh
 * Process graph decomposition into SCCs including types */
#ifndef _DIVINE_DVE_PROCESS_DECOMPOSITION_HH_
#define _DIVINE_DVE_PROCESS_DECOMPOSITION_HH_
 
#ifndef DOXYGEN_PROCESSING
#include "common/process_decomposition.hh"
#include "system/dve/dve_explicit_system.hh"
#include "system/dve/dve_process.hh"
namespace divine {
#endif
  
  /*! This class is used to decompose a graph of a single process specified
   *  in a dve file into SCCs. The decomposition is not accesible directly
   *  but with member functions returning for a given state SCC id and
   *  type. */
  class dve_process_decomposition_t : public process_decomposition_t
  {
  private:
    dve_explicit_system_t& system;
    bool initialized;
    const dve_process_t *dve_proc_ptr;
    std::size_t proc_id;
    int proc_size;
    int scc_count;

    int n,h;
    int *graf;
    int *hran;
    int pocitadlo;
    int *number, *low;
    int *zasobnik;
    int ps; 
    int *on_stack;
    int *komp;
    int *types;

    void strong(int);
  public:
    dve_process_decomposition_t(dve_explicit_system_t& system);
    ~dve_process_decomposition_t();
    
    void parse_process(std::size_t);
    int get_process_scc_id(state_t&);
    int get_process_scc_type(state_t&);    
    int get_scc_type(int);
    int get_scc_count();
    bool is_weak();
};


#ifndef DOXYGEN_PROCESSING
}
#endif
#endif

/*!\file
 * The main contribution of this file is the class bymoc_system_t
 */
#ifndef DIVINE_BYMOC_SYSTEM_HH
#define DIVINE_BYMOC_SYSTEM_HH

#ifndef DOXYGEN_PROCESSING
#include <fstream>
#include "system/system.hh"
#include "system/explicit_system.hh" //because of succ_container_t
#include "system/bymoc/bymoc_expression.hh"
#include "common/array.hh"
#include "common/error.hh"
#include "common/stateallocator.hh"
#include "system/bymoc/vm/nipsvm.h"
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

#define NIPSVM_STATE(s) reinterpret_cast<nipsvm_state_t *>(s)

//The main DiVinE namespace - we do not want Doxygen to see it
namespace divine {
#endif //DOXYGEN_PROCESSING

//!Class for Promela/Bytecode system representation
/*!This class implements the abstract interface system_t
 *
 * This implementation is based on external virtual machine for
 * special bytecode. Therefore this \sys is called BYMOC \sys.
 *
 * It supports only very basic functionality of system_t interface
 * (processes, transition and expressions are not supported).
 * The calls of non-implemented methods cause error messsages.
 */
class bymoc_system_t: virtual public system_t
 {
     friend class bymoc_process_decomposition_t;
  protected:
 
  nipsvm_t nipsvm;
 
  public:

  //!A constructor.
  /*!\param estack =
   * the <b>error vector</b>, that will be used by created instance of system_t
   */
  bymoc_system_t(error_vector_t & evect = gerr);
  //!A destructor.
  virtual ~bymoc_system_t();

  /*! @name Obligatory part of abstact interface
     These methods have to implemented in each implementation of system_t*/
  //!Warning - this method is still not implemented - TODO
  virtual slong_int_t read(std::istream & ins = std::cin);
  //!Implements system_t::read(const char * const filename) in BYMOC \sys
  virtual slong_int_t read(const char * const filename);
  //!Warning - this method is still not implemented - TODO
  virtual slong_int_t from_string(const std::string str);
  //!Warning - this method is still not implemented - TODO
  virtual bool write(const char * const filename);
  //!Warning - this method is still not implemented - TODO
  virtual void write(std::ostream & outs = std::cout);
  //!Warning - this method is still not implemented - TODO
  virtual std::string to_string();
  /*@}*/

  
  ///// BYMOC SYSTEM CANNOT WORK WITH PROPERTY PROCESS: /////
  /*! @name Methods working with property process
    These methods are not implemented and can_property_process() returns false
   @{*/
  //!Not imlemented in BYMOC \sys - throws error message
  virtual process_t * get_property_process();
  //!Not imlemented in BYMOC \sys - throws error message
  virtual const process_t * get_property_process() const;
  //!Not imlemented in BYMOC \sys - throws error message
  virtual size_int_t get_property_gid() const;
  //!Not imlemented in BYMOC \sys - throws error message
  virtual void set_property_gid(const size_int_t gid);
  /*@}*/

  
  ///// BYMOC SYSTEM CANNOT WORK WITH PROCESSES: /////
  /*!@name Methods working with processes
    These methods are not implemented and can_processes() returns false.
   @{*/
  //!Not imlemented in BYMOC \sys - throws error message
  virtual size_int_t get_process_count() const;
  //!Not imlemented in BYMOC \sys - throws error message
  virtual process_t * get_process(const size_int_t gid);
  //!Not imlemented in BYMOC \sys - throws error message
  virtual const process_t * get_process(const size_int_t id) const;
  //!Not implemented in BYMOC \sys - throws error message
  virtual property_type_t get_property_type();
  /*@}*/


  ///// BYMOC SYSTEM CANNOT WORK WITH TRANSITIONS: /////
  /*!@name Methods working with transitions
    These methods are not implemented and can_transitions() returns false.
   @{*/
  //!Not imlemented in BYMOC \sys - throws error message
  virtual size_int_t get_trans_count() const;
  //!Not imlemented in BYMOC \sys - throws error message
  virtual transition_t * get_transition(size_int_t gid);
  //!Not imlemented in BYMOC \sys - throws error message
  virtual const transition_t * get_transition(size_int_t gid) const;
  /*@}*/
  
  ///// BYMOC SYSTEM CANNOT BE MODIFIED: /////
  /*!@name Methods modifying a system
     These methods are not implemented and can_be_modified() returns false.
   @{*/
  //!Not imlemented in BYMOC \sys - throws error message
  virtual void add_process(process_t * const process);
  //!Not imlemented in BYMOC \sys - throws error message
  virtual void remove_process(const size_int_t process_id);
  /*@}*/

 };

struct successor_state_ctx
{
 successor_state_ctx (succ_container_t& succs) : succs (succs), succ_generation_type ( SUCC_NORMAL ) {};
 succ_container_t& succs;
 int succ_generation_type;
 bymoc_system_t *system;
};

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#endif //DOXYGEN_PROCESSING

#endif

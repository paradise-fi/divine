/*!\file
 * The main contribution of this file is the abstact interface system_t -
 * the class representing a model of the \sys.
 */
#ifndef DIVINE_SYSTEM_HH
#define DIVINE_SYSTEM_HH

#ifndef DOXYGEN_PROCESSING
#include <fstream>
#include "common/array.hh"
#include "common/error.hh"
#include "system/system_abilities.hh"
#include "system/data.hh"
#include "system/transition.hh"
#include "system/process.hh"
#include "system/state.hh"
#include "common/stateallocator.hh"

//The main DiVinE namespace - we do not want Doxygen to see it
namespace divine {
#endif //DOXYGEN_PROCESSING


//property-type type definition
enum property_type_t {NONE, BUCHI, GENBUCHI, MULLER, RABIN, STREETT};
//predeclarations of classes:
class transition_t;
class process_t;
class process_decomposition_t;
    
//!Abstract interface of a class representing a model of a system
/*!This class provides an interface to the model of system.
 * Depending on abilities (see get_abilities()) you can
 * more or less deeply analyse the model.
 *
 * Furthermore you can generate states of the system using
 * child class explitcit_system_t (and its children)
 */
class system_t : public LegacyStateAllocator
 {
  protected:
    error_vector_t & terr;
    /* abilities can be modified in successors of system_t */
    system_abilities_t abilities;
    bool with_property;
    
    void copy_private_part(const system_t & second)
    {
     abilities = second.abilities;
     with_property = second.with_property;
    }
    
  public:
  static const ERR_type_t ERR_TYPE_SYSTEM;
  static const ERR_id_t ERR_FILE_NOT_OPEN;
  
  //!A constructor.
  /*!\param evect =
   * the <b>error vector</b>, that will be used by created instance of system_t
   */
  system_t(error_vector_t & evect = gerr):terr(evect)
   {
    //system is defautly as dumb as possible
    with_property = NONE;
   };
  //!A destructor.
  virtual ~system_t() {};
  //!Returns a reference to the error_vector_t inside of system_t
  error_vector_t & get_error_vector() { return terr; }
  //!Returns a constant reference to the error_vector_t inside of system_t
  const error_vector_t & get_error_vector() const { return terr; }
  //!Returns a structure storing a list of abilities of current system
  /*!It also stores the list of abilites of processes and transitions,
   * which are a part of a \sys and abilities of \exp_sys */
  system_abilities_t & get_abilities() { return abilities; }
  //!Returns a structure storing a list of abilities of current system
  /*!It also stores the list of abilites of processes and transitions,
   * which are a part of a \sys and abilities of \exp_sys */
  const system_abilities_t & get_abilities() const { return abilities; }
   
  //!Sets, wheter \sys is the \sys with a property process.
  void set_with_property(const bool is_with) { with_property=is_with; }

  //!Returns, whether a property process is specified or not.
  bool get_with_property() const { return (with_property); }   

  /*! @name Obligatory part of abstact interface
     These methods have to implemented in each implementation of system_t*/

   //!Method for identifying type of the accepting condition of the property
   //!process. Possible types are NONE, BUCHI, GENBUCHI, MULLER, RABIN, STREETT.
   virtual property_type_t get_property_type() = 0;

  //!Method for reading a model of a \sys from a stream 
  /*!\param ins = input stream containing a source of a model
   * \return ... 0 iff no error occurs, non-zero value in a case of error
   *             during a reading.
   */
  virtual slong_int_t read(std::istream & ins = std::cin) = 0;
  //!Method for reading a model of a \sys from a file
  /*!\param filename = path to a source of a model
   * \return 0 iff successfuly opens the file and successfuly parses it.
   *         Otherwise it returns non-zero value. If the error was caused by
   *         the impossibility to open a file, it returns
   *         system_t::ERR_FILE_NOT_OPEN.
   */
  virtual slong_int_t read(const char * const filename) = 0;
  //!Method for reading a model of a \sys from a source stored in a string
  /*!\param str = string to read
   * \return ... 0 iff no error occurs, non-zero value in a case of error
   *             during a reading.
   */
  virtual slong_int_t from_string(const std::string str) = 0;
  //!Method for writing currently stored model of a \sys to a file
  /*!\param filename = path to a file
   * \return true iff successfuly opens the file */
  virtual bool write(const char * const filename) = 0;
  //!Method for writing currently stored model of a \sys to a stream.
  //! (parameter `outs')
  virtual void write(std::ostream & outs = std::cout) = 0;
  //!Method for writing source of actually stored model of a \sys
  //! to the string
  virtual std::string to_string() = 0;
  /*@}*/
  
  //!Tells, whether current \sys is able to work with property process
  bool can_property_process() const
  { return abilities.system_can_property_process; }
  
  /*! @name Methods working with property process
    These methods are implemented only if can_property_process()
    returns true.
   @{*/
  
  //!Returns a pointer to the property process.
  /*!This method is implemented only if can_property_process() returns true.*/
  virtual process_t * get_property_process() = 0;
  
  //!Returns a pointer to the constant instance of property process.
  /*!This method is implemented only if can_property_process() returns true.*/
  virtual const process_t * get_property_process() const = 0;
  
  //!Returns \GID of property process.
  /*!This method is implemented only if can_property_process() returns true.*/
  virtual size_int_t get_property_gid() const = 0;
  
  //!Sets, which process is a property process.
  /*!\param gid = \GID of new property process
   * 
   * This method is implemented only if can_property_process() returns true.*/
  virtual void set_property_gid(const size_int_t gid) = 0;
  
  /*@}*/

  

  //!Tells, whether current \sys is able to work with processes
  bool can_decompose_property() const { return abilities.system_can_decompose_property; }

  /*!@name Methods to check the SCCs of property process graph
   @{*/

   //! Returns property decomposition, or 0, if subsystem is not available.
   virtual process_decomposition_t *get_property_decomposition()
   {
       return 0;
   }
   /*@}*/

  //!Tells, whether current \sys is able to work with processes
  bool can_processes() const { return abilities.system_can_processes; }

  /*!@name Methods working with processes
    These methods are implemented only if can_processes()
    returns true.
   @{*/
  
  //!Returns a count of processes in the \sys.
  /*!This method is implemented only if can_processes_true_methods()
   * returns true.*/
  virtual size_int_t get_process_count() const = 0;
  //!Returns a process with \proc_GID `gid'
  /*!\param gid = \proc_GID of a process (can be any from an interval
   *              0..(get_process_count()-1) )
   * \return process (represented by a pointer to process_t)
   * 
   * This method is implemented only if can_processes_true_methods()
   * returns true.*/
  virtual process_t * get_process(const size_int_t gid) = 0;
  //!Returns a constant instance of a process with \proc_GID `gid'
  /*!\param gid = \proc_GID of a process (can be any from an interval
   *              0..(get_process_count()-1) )
   * \return process (represented by a pointer to process_t)
   * 
   * This method is implemented only if can_processes_true_methods()
   * returns true.*/
  virtual const process_t * get_process(const size_int_t gid) const = 0;

  /*@}*/
  
  //!Tells, whether current \sys is able to work with transitions
  bool can_transitions() const { return abilities.system_can_transitions; }
  
  /*!@name Methods working with transitions
    These methods are implemented only if can_transitions() returns true.
   @{*/
  
  //!Returns the count of all transitions in the \sys.
  /*!This method is implemented only if can_transitions() returns true.
   * \note Returned value is the sum of returned values obtained by calling
   * process_t::get_trans_count() for all processes of the system*/
  virtual size_int_t get_trans_count() const = 0;
  //!Returns a transition with \trans_GID = `gid'
  /*!This method is implemented only if can_transitions() returns true.*/
  virtual transition_t * get_transition(size_int_t gid) = 0;
  //!Returns a constant instance of transition with \trans_GID = `gid'
  /*!This method is implemented only if can_transitions() returns true.*/
  virtual const transition_t * get_transition(size_int_t gid) const = 0;
  
  /*@}*/
  
  //!Tells, whether current \sys can be modified
  /*!Generally thr \sys can be modified using add_process() and
   * remove_process() together with methods modifying processes*/
  bool can_be_modified() const { return abilities.system_can_be_modified; }
  
  /*!@name Methods modifying a system
     These methods are implemented only if can_be_modified() returns true.
   @{*/
  
  //!Method for adding of process to \sys
  /*!This method is implemented only if can_be_modified() returns true.*/
  virtual void add_process(process_t * const process) = 0;
  //!Method for removing of process from \sys
  /*!This method is implemented only if can_be_modified() returns true.*/
  virtual void remove_process(const size_int_t process_id) = 0;

  /*@}*/
 };

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#endif //DOXYGEN_PROCESSING

#endif


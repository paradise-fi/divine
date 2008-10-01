/*!\file
 * The main contribution of this file is the abstact interface process_t.
 */
#ifndef DIVINE_PROCESS_HH
#define DIVINE_PROCESS_HH

#ifndef DOXYGEN_PROCESSING

#include "system/system.hh"

namespace divine { //We want Doxygen not to see namespace `dve'

#endif //DOXYGEN_PROCESSING

//predeclarations of classes:
class system_t;
class transition_t;

//!Abstact interface of a class representing a process of a \sys
/*!This class represents a process of a \sys. Its "parent" \sys
 * is given in a constructor process_t(system_t * const system)
 * or a method set_parent_system().
 *
 * \note Developer is responsible
 * for correct setting of corresponding "parent" \sys (but he
 * rarely needs to create own processes - they are usually created
 * automatically during reading of a source of the system from a file)
 */
class process_t
{
 protected:
 //!Static protected data item storing an \evect used in case of any
 //! error messages
 static error_vector_t * pproc_terr;
 //!Protected static method setting \evect, which
 //! will be used in case of any error message
 static void set_error_vector(error_vector_t & evect)
   { pproc_terr = &evect; }
 //!Protected static method returning \evect, which
 //! will be used in case of any error message
 static error_vector_t & get_error_vector() { return (*pproc_terr); }
 
 //!Protected data item storing a parent \sys
 system_t * parent_system;
 //!Protected data item storing \GID of this process
 size_int_t gid;

 public:
 //!A constructor
 process_t() { gid=NO_ID; parent_system=0; }
 //!A constructor
 /*!\param system = "parent" \sys of this process*/
 process_t(system_t * const system) { gid=NO_ID; parent_system=system; }
 //!A destructor
 virtual ~process_t() {}//!<A destructor

 //!Returns a \GID of this process
 size_int_t get_gid() const { return gid; }
 //!Returns a "parent" \sys of this process
 system_t * get_parent_system() const { return parent_system; }
 //!Sets a "parent" \sys of this process
 virtual void set_parent_system(system_t & system)
  { parent_system = &system; }
 /*! @name Obligatory part of abstact interface */
 //!Returns a string representation of the process.
 virtual std::string to_string() const = 0;
 //!Writes a string representation of the process to a stream
 virtual void write(std::ostream & ostr) const = 0;
 /*@}*/
 
 //!Tells, whether process can work with transitions and contains them
 bool can_transitions() const;

 /*! @name Methods working with transitions of a process
   These methods are implemented only if can_transitions() returns true.
  @{*/

 //!Returns a pointer to the transition with \LID `lid'.
 /*!\LID of transitions can be from 0 to (get_trans_count()-1)*/
 virtual transition_t * get_transition(const size_int_t lid) = 0;
 //!Returns a pointer to the constant transition with \LID `lid'.
 /*!\LID of transitions can be from 0 to (get_trans_count()-1)*/
 virtual const transition_t * get_transition(const size_int_t lid) const = 0;
 //!Returns a count of transitions of this process
 /*!Then \LID of transitions can be from 0 to (get_trans_count()-1)
  * \note
  * The sum of returned values obtained by calling this method for all
  * processes of the system is equal to the value obtained by method
  * system_t::get_trans_count()*/
 virtual size_int_t get_trans_count() const = 0;
  
 /*@}*/

 
 //!Tells, whether processes can be modified
 bool can_be_modified() const;

 /*!  @name Methods modifying a process
   These methods are implemented only if can_transitions() returns true.
  @{*/

 //!Sets a \GID of this process
 virtual void set_gid(const size_int_t new_gid) { gid = new_gid; }
 
 //!Adds new transition to the process
 /*!\param transition = pointer to the transition to add
  *
  * This method modifies the added
  * transition because it has to set \trans_LID and \part_ID.
  */
 virtual void add_transition(transition_t * const transition) = 0;
 //!Removes a transition from the process
 /*!\param transition_gid = \GID of removed transition*/
 virtual void remove_transition(const size_int_t transition_gid) = 0;
 
 /*@}*/
 
 //!Tells, whether process can be read from a string representation
 bool can_read() const;
  
 /*!  @name Methods for reading a process from a string representation
   These methods are implemented only if can_read() returns true.
  @{*/
 
 //!Reads in the process from the string representation
 /*!\param proc_str = string to read from
  * \return ... 0 iff no error occurs, non-zero value in a case of error
  *             during a reading.
  */
 virtual int from_string(std::string & proc_str) = 0;
 //!Reads in the process from the string representation in stream
 /*!\param istr = input stream containing source of the process
  * \return ... 0 iff no error occurs, non-zero value in a case of error
  *             during a reading.
  */
 virtual int read(std::istream & istr) = 0;
 
 /*@}*/
};


#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE

#endif //DOXYGEN_PROCESSING

#endif





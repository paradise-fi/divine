/* \file
 * The main contribution of this file is the abstact interface expression_t.
 */
#ifndef DIVINE_TRANSITION_HH
#define DIVINE_TRANSITION_HH

#include "system/system.hh"

#ifndef DOXYGEN_PROCESSING

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

//predeclarations of classes:
class system_t;

//definition of the main class of this file:
//!Abstract interface of a class representing a transition
/*!This class represents a transition. It is a part of the model of
 * a system. The corresponding \sys is given as a parameter of
 * constructor transition_t(system_t * const system) or by method
 * set_parent_system(). Developer is reponsible for correct setting
 * of this "parent" \sys (but developer rarely creates new transitions - 
 * they are automatically created by \sys during source file reading).
 */
class transition_t
{
 protected:
 //!Protected static data item containing \evect, which
 //! will be used in case of any error message
 static error_vector_t * ptrans_terr;
 //!Protected static method setting \evect, which
 //! will be used in case of any error message
 static void set_error_vector(error_vector_t & evect)
   { ptrans_terr = &evect; }
 //!Protected static method returning \evect, which
 //! will be used in case of any error message
 static error_vector_t & get_error_vector() { return (*ptrans_terr); }
 
 size_int_t local_id; //!<Protected data item containing \LID
 size_int_t global_id; //!<Protected data item containing \GID
 system_t * parent_system; //!<Protected data item containing "parent" \sys
 
 public:
 //!A constructor
 transition_t() { local_id=NO_ID; global_id=NO_ID; parent_system=0; }
 //!A constructor
 /*!\param system = "parent" \sys of this transition*/
 transition_t(system_t * const system)
 { local_id=NO_ID; global_id=NO_ID; parent_system=system;}
 //!A destructor
 virtual ~transition_t() {}
 //!Returns \trans_LID of this transition
 /*!\note This method is not virtual, because it should be really fast*/
 size_int_t get_lid() const { return local_id; }
 //!Returns \trans_GID of this transition
 /*!\note This method is not virtual, because it should be really fast*/
 size_int_t get_gid() const { return global_id; }
 //!Returns a "parent" \sys of this transition
 system_t * get_parent_system() const { return parent_system; }
 //!Sets a "parent" \sys of this transition
 virtual void set_parent_system(system_t & system)
 { parent_system = &system; }
 
 //!Tells, whether this transition can be modified
 /*!This method uses "parent" \sys given in a constructor or a method
  * set_parent_system(). Therefore "parent" \sys has to be set before the
  * first call of this method.
  */
 bool can_be_modified() const;
 
 /*!@name Methods for modifying a transition
   These methods are implemented only if can_be_modified()
   returns true.
  @{*/
 
 //!Sets a \GID of this transition
 /*!\warning See the description of concrete implementation. This method
  * often cannot be used by developer from consistency reasons (even if
  * it is implemented).
  */
 virtual void set_gid(const size_int_t gid) { global_id = gid; }
 //!Sets a \LID of this transition
 /*!\warning See the description of concrete implementation. This method
  * often cannot be used by developer from consistency reasons (even if
  * it is implemented).
  */
 virtual void set_lid(const size_int_t id) { local_id = id; }
 /*@}*/
 
 ///// IO Write functions - output functions are obligatory if /////
 ///// system can work with transitions                        /////
 /*! @name Obligatory part of abstact interface
    These methods have to implemented in each implementation of transition_t
    (if transitions are supported by the system).*/
 //
 //!Returns a string representation of the transition.
 /*!If \sys can work with transitions (according to
  * system_t::can_transitions()), this method is obligatory to implement.
  */
 virtual std::string to_string() const = 0;
 //!Writes a string representation of the transition to stream
 /*!If \sys can work with transitions (according to
  * system_t::can_transitions()), this method is obligatory to implement.
  */
 virtual void write(std::ostream & ostr) const = 0;
 /*@}*/
 
 //!Tells, whether this transition can be modified
 /*!This method uses "parent" \sys given in a constructor or a method
  * set_parent_system(). Therefore "parent" \sys has to be set before the
  * first call of this method.
  */
 bool can_read() const;

 
 ///// IO Read functions - only for the case of can_read == true /////
 /////                                                           /////
 //
 
 /*! @name Methods for reading a transition from a string representation
   These methods are implemented only if can_read() returns true.
  @{*/
  
 //!Reads in the transition from a string representation in stream
 /*!\param istr = input stream containing a source of transition
  * \param process_gid = context of process (default value NO_ID = global
  *                      context)
  * \return ... 0 iff no error occurs, non-zero value in a case of error
  *             during a reading.
  */
 virtual int read(std::istream & istr, size_int_t process_gid = NO_ID) = 0;
 //!Reads in the transition from a string representation
 /*!\param trans_str = string containing a source of a transition
  * \param process_gid = context of a process (default value NO_ID = global
  *                      context)
  * \return ... 0 iff no error occurs, non-zero value in a case of error
  *             during a reading.
  */
 virtual int from_string(std::string & trans_str,
                 const size_int_t process_gid = NO_ID) = 0;
 /*@}*/

};

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE

#endif //DOXYGEN_PROCESSING

#endif


/*!\file
 * This file contains:
 * - definition of abbstract interface system_trans_t, which represents
 *   a transition of the \sys (composed from one or more transition_t -
 *   synchronized transitions)
 * - simple extension of system_trans_t called enabled_trans_t,
 *   which is used to represent a system transition, which
 *   is enabled (it is only extented by the possibility of being erroneous). 
 * - definition of a container for storing
 *   of enabled transitions (based on array_of_abstract_t).
 */
#ifndef DIVINE_SYSTEM_TRANS_HH
#define DIVINE_SYSTEM_TRANS_HH

#ifndef DOXYGEN_PROCESSING
#include "system/transition.hh"
#include "system/explicit_system.hh"
#include "common/array_of_abstract.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

class explicit_system_t;

//!Class storing informations about one system transition
/*!System transition consists of several transitions
 * (of type transition_t, iff \sys can work with transitions)
 *
 * System transition represent the step of the entire \sys (compared to
 * the transition_t, which represents the step of a single process).
 *
 * Developer will usually use system_trans_t as its child enabled_trans_t
 * which adds the "erroneous" property to this class.
 */
class system_trans_t
 {
  public:
   //!A constructor
   system_trans_t() {}
   //!A destructor
   virtual ~system_trans_t() {}
   //!An assignment operator
   /*!Makes a hard copy of system transition => takes a time O(second.size())*/
   virtual system_trans_t & operator=(const system_trans_t & second) = 0;
   //!Returns a string representation of enabled transition
   virtual std::string to_string() const = 0;
   //!Prints a string representation of enabled trantition to output stream
   //! `ostr'
   virtual void write(std::ostream & ostr) const = 0;
   
  /*! @name Methods accessing transitions forming a system transition
    These methods are implemented only if system_t::can_transitions()
    in the system that has generated the instance of this class returns true.
   @{*/
   
   //!Sets a count of transitions of processes forming this enabled transition
   virtual void set_count(const size_int_t new_count) = 0;
   //!Returns a count of transitions of processes forming this enabled
   //! transition
   virtual size_int_t get_count() const = 0;
   //!Returns `i'-th transition forming this transition of the system
   virtual transition_t *& operator[](const int i) = 0;
   //!Returns `i'-th transition forming this transition of the system
   virtual transition_t * const & operator[](const int i) const = 0;
  /*}@*/
 };

//!Class storing informations about one enabled transition
/*!Enabled transition = system transition + erroneous property
 *
 * Enabled transitions can be produced by \exp_sys (explicit_system_t),
 * if it can work with enabled transitions.
 *
 * Each enabled transition represents a possible step of a \sys in a given
 * state of the \sys.
 */
class enabled_trans_t: public virtual system_trans_t
{
 private:
 bool error;                 //!<Determines whether there was an error during
                             //!< the computation of enabled transition
 public:
 //!A constructor
 enabled_trans_t() { error = false; }
 //!An assignment operator
 virtual enabled_trans_t & operator=(const enabled_trans_t & second) = 0;
 //!Sets, whether an enabled transition is erroneous
 void set_erroneous(const bool is_errorneous) { error = is_errorneous; }
 //!Returns, whether an enables transition is erroneous
 bool get_erroneous() const { return error; }
};


//enabled_trans_t * system_new_field_of_objects(const void * params, const size_int_t count);

enabled_trans_t * system_new_enabled_trans(const void * params);

//!Container determined for storing enabled processes in one state
/*!This container should be used in calls of explicit_system_t::get_succs()
 * and explicit_system_t::get_enabled_trans() functions for storing the list
 * of transitions enabled in a given state. Its contructor has an instance
 * of explicit_system_t as a parameter, because it tries to guess the
 * size of memory sufficient to store maximal count of transitions,
 * which are enabled in one state of the system. This guess prevent the
 * big count of reallocations at the beginning of the run.
 *
 * If the system is with property process, then the property_succ_count must be
 * set (by set_property_succ_count() function) to correct usage of this
 * container.
 *
 * \note See array_of_abstract_t for details and more methods of this class*/
class enabled_trans_container_t: public array_of_abstract_t<enabled_trans_t, system_new_enabled_trans>
 {
  private:
  size_int_t property_succ_count;
  size_int_t * trans_bounds;
  
  public:
  //!A constructor
  enabled_trans_container_t(const explicit_system_t & system);
  //!A destructor
  ~enabled_trans_container_t();
  //!Sets, where the list of enabled transitions of next process begins.
  /* Sets, where the list of enabled transitions of process with \GID
   * \a process_gid begins.
   *
   * If \a process_gid is equal to the number of processes (so it is larger
   * than arbitrary \proc_GID), it serves only as a bound of the list of
   * transitions of the last process */
  void set_next_begin(const size_int_t process_gid,
                      const size_int_t next_begin)
  { trans_bounds[process_gid+1] = next_begin; }
  //!Returns the index of first enabled transition of process with GID
  //! `process_gid'.
  size_int_t get_begin(const size_int_t process_gid) const
  { return trans_bounds[process_gid]; }
  //!Returns a count of transitions of process with GID `process_gid'
  size_int_t get_count(const size_int_t process_gid) const
  { return (get_begin(process_gid+1)-get_begin(process_gid)); }
  //!Returns a pointer to the enabled transition of process with GID
  // `process_gid'.
  enabled_trans_t * get_enabled_transition
     (const size_int_t process_gid, const size_int_t index)
  { return &(*this)[get_begin(process_gid)+index]; }
  //!The definition of get_enabled_transition() for the case of constant
  // instance of this class.
  const enabled_trans_t * get_enabled_transition
     (const size_int_t process_gid, const size_int_t index) const
  { return &(*this)[get_begin(process_gid)+index]; }
  //!Empties whole contents of container
  void clear()
  { array_of_abstract_t<enabled_trans_t, system_new_enabled_trans>::clear();
    property_succ_count = 0; }
  //!Sets a count of transitions enabled in a property process
  void set_property_succ_count(const size_int_t count)
  { property_succ_count = count; }
  //!Returns a count of transitions enabled in a property process
  size_int_t get_property_succ_count() const { return property_succ_count; }
 };

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE

#endif //DOXYGEN_PROCESSING

#endif


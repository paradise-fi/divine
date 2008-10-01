/*!\file
 * This file contains definitions of classes dve_system_trans_t and
 * dve_enabled_trans_t.*/
#ifndef DIVINE_DVE_SYSTEM_TRANS_HH
#define DIVINE_DVE_SYSTEM_TRANS_HH

#ifndef DOXYGEN_PROCESSING
#include "system/dve/dve_system_trans.hh"
#include "system/dve/dve_explicit_system.hh"
#include "system/dve/dve_transition.hh"
#include "system/system_trans.hh"
#include "common/array.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

class explicit_system_t;


//!Class implementing system trasition in DVE \sys
class dve_system_trans_t: virtual public system_trans_t
 {
  private:
   transition_t ** trans_list;  //!<List of synchronized transitions of the
                               //!<processes forming the transtion of the system
   size_int_t count;          //!<Count of stored transitions

  protected:
   void create_from(const system_trans_t & second);
   void copy_from(const system_trans_t & second)
   { if (trans_list) delete [] trans_list; create_from(second); }

  public:
   //!A constructor
   dve_system_trans_t() { trans_list = 0; count = 0; }
   //!A copy constructor
   dve_system_trans_t(const dve_system_trans_t & second) : system_trans_t()
   { create_from(second); }
   //!A destructor
   virtual ~dve_system_trans_t();
   //!Implements system_trans_t::operator=() in DVE \sys
   virtual system_trans_t & operator=(const system_trans_t & second);
   //!Implements system_trans_t::to_string() in DVE \sys
   virtual std::string to_string() const;
   //!Implements system_trans_t::write() in DVE \sys
   virtual void write(std::ostream & ostr) const;

   //// DVE system can work with transitions ////
  /*! @name Methods accessing transitions forming a system transition
    These methods are implemented and system_t::can_transitions()
    in the system that has generated the instance of this class returns true.
   @{*/
   //!Implements system_trans_t::set_count() in DVE \sys
   virtual void set_count(const size_int_t new_count);
   //!Implements system_trans_t::get_count() in DVE \sys
   virtual size_int_t get_count() const { return count; }
   //!Implements system_trans_t::operator[](const int i)
   virtual transition_t *& operator[](const int i) { return trans_list[i]; }
   //!Implements system_trans_t::operator[](const int i) const in DVE \sys
   virtual transition_t * const & operator[](const int i) const
     { return trans_list[i]; }
  /*}@*/
 };

//!Class implementing enabled trasition in DVE \sys
class dve_enabled_trans_t: public enabled_trans_t, public dve_system_trans_t 
{
 private:
 bool error;                 //!<Determines whether there was an error during
                             //!< the computation of enabled transition
 public:

 //// CONSTRUCTORS ////
 //!A constructor
 dve_enabled_trans_t() {}
 
 //!A copy constructor
 dve_enabled_trans_t(const dve_enabled_trans_t & second);
 
 //// VIRTUAL INTERFACE OF ENABLED_TRANS_T ////
 //!Implements enabled_trans_t operator=() in DVE \sys
 virtual enabled_trans_t & operator=(const enabled_trans_t & second);
};


#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE

#endif //DOXYGEN_PROCESSING

#endif


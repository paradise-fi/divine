/*!\file
 *  state hashing support
 *
 *  \author Jiri Barnat
 */
#ifndef _DIVINE_EXPLICIT_STORAGE_HH_
#define _DIVINE_EXPLICIT_STORAGE_HH_


#ifndef DOXYGEN_PROCESSING
#include "system/state.hh"
#include "system/explicit_system.hh"
#include "storage/compressor.hh"
#include "common/error.hh"
#include "common/hash_function.hh"

namespace divine {
#endif //DOXYGEN_PROCESSING

#define EXPLICIT_STORAGE_ERR_TYPE 11
#define EXPLICIT_STORAGE_HASH_SEED 0xBA9A9ABA


  //!State reference class
  /*!This class is a constant-sized short representation of state stored
   * in an instance of explicit_storage_t.
   *
   * explicit_storage_t guaranties that this reference is a unique identifier
   * of a state for each instance of explicit_storage_t.
   *
   * Operators ==, !=, <, <= and > and >= are defined for this class.
   *
   * References are also useful in distributed environment in a connection
   * with the identifier of computer that keeps the referenced state.
   * It suffices to send a reference and computer ID instead of relatively
   * long explicit representation of a state.
   */
  class state_ref_t{
  public:
    state_ref_t();
    size_t hres;
    size_t id;
    std::string to_string();

    /*! Invalidates the reference, i.e. make it hold an invalid value. */
    void invalidate();

    /*! Tests whether the reference holds a valid value or not. */
    bool is_valid();      
  };
  
  //! Prints a reference representation to the given stream.
  inline std::ostream & operator<<(std::ostream & ostr, const state_ref_t & ref)
  { ostr << '[' << ref.hres << ", " << ref.id << ']'; return ostr; }
  
  //!Operator for equality of references.
  inline static bool operator==(const state_ref_t & r1, const state_ref_t & r2)
  {
   if (memcmp(&r1,&r2,sizeof(state_ref_t))==0) return true;
   else return false;
  }
  
  //!Operator for non-equality of references.
  inline static bool operator!=(const state_ref_t & r1, const state_ref_t & r2)
  {
   if (memcmp(&r1,&r2,sizeof(state_ref_t))!=0) return true;
   else return false;
  }
  
  //!Operator "<" for references
  inline static bool operator<(const state_ref_t & r1, const state_ref_t & r2)
  {
   if (memcmp(&r1,&r2,sizeof(state_ref_t))<0) return true;
   else return false;
  }
    
  //!Operator "<=" for references
  inline static bool operator<=(const state_ref_t & r1, const state_ref_t & r2)
  {
   if (memcmp(&r1,&r2,sizeof(state_ref_t))<=0) return true;
   else return false;
  }
  
  //!Operator ">" for references
  inline static bool operator>(const state_ref_t & r1, const state_ref_t & r2)
  {
   if (memcmp(&r1,&r2,sizeof(state_ref_t))>0) return true;
   else return false;
  }
    
  //!Operator ">=" for references
  inline static bool operator>=(const state_ref_t & r1, const state_ref_t & r2)
  {
   if (memcmp(&r1,&r2,sizeof(state_ref_t))>=0) return true;
   else return false;
  }
    
    
    
  //! explicit storage class
  class explicit_storage_t {
  public:

    /*! Constructor. An error_vector_t may be specified. */
    explicit_storage_t(divine::error_vector_t& = gerr);
    
    /*! This member function performs initialization of the hash table. It
     *  must be called in order to use the class for storing states. This
     *  function call may be predecessed by calls of various set functions.
     *  */
    void init(divine::explicit_system_t&); 
    
    /*! Insert a copy of the given state in the set of visited states. An
     *  exception is generated in the case of unseccesfull insert. */
    void insert(state_t);

    /*! Insert a copy of the given state in the set of visited states. An
     *  exception is generated in the case of unsuccesfull insert. Also sets
     *  the given state_ref_t to reference the newly inserted state. */
    void insert(state_t,state_ref_t&);

    /*! Tests whether given state is stored in the set of visited states.
     *  In the case the state is present in the set, the function returns
     *  true and sets the given state_ref_t to reference the state in the
     *  set, otherwise a copy of the state is inserted in the set, the given
     *  state_ref_t is set to the state that has been just inserted, and
     *  false is returned. In the latter case an exception is generated in
     *  the case of unseccesfull insert. */
    bool is_stored_if_not_insert(state_t,state_ref_t&);

    /*! Returns true if the state is inserted in the set of visited states. */
    bool is_stored(state_t);

    /*! Returns true if the state is inserted in the set of visited states
     *  and if so then sets the state_ref_t to reference the state. */
    bool is_stored(state_t,state_ref_t&);

    /*! Deletes the state that is referenced by given state_ref_t. Returns
     *  false if the delete operation was unsuccessful. */
    bool delete_by_ref(state_ref_t);

    /*! Deletes all stored states and collision lists. Method has a
     * voluntary parameter leave_collision_lists. If called with
     * leave_collision_lists = true, then colission lists are only cleared
     * and remain allocated. */
    void delete_all_states(bool leave_collision_lists = false);

    /*! Reconstructs state referenced by given state_ref_t. Invalid
     *  reference will cause warning and crash the application. */
    state_t reconstruct(state_ref_t); 

    /*! Returns current number of bytes allocated by storage and compressor
     *  instances. Note this differ from real memory consumption as no
     *  fragmentation is included. */
    size_t get_mem_used();

    /*! Returns maximal (up to the time if this call) number of bytes
     *  allocated by storage and compressor instances. Note this differ from
     *  real memory consumption as no fragmentation is included. */
    size_t get_mem_max_used();

    /*! Returns current number of states stored in the set. */
    size_t get_states_stored();      

    /*! Returns maximum number of states stored in the set. */
    size_t get_states_max_stored();

    /*! Returns sum of sizes of collision tables (sum of lengths of all collision lists). */
    size_t get_coltables();

    /*! Returns maximum size of a single collision table (maximum length of a collision list). */
    size_t get_max_coltable();   

    /*! Returns the number of occupied lines (lines with at least one state
     *  stored) in the hashtable. */
    size_t get_ht_occupancy();        

    /*! Sets compression method. Where the possibilities are: NO_COMPRESS
     * (default) and HUFFMAN_COMPRESS. The call of this function must
     * preceed the call of init member function. */
    void set_compression_method(size_t);

    /*! Sets the hash table size (default: 2^16 = 65536). The call of this
     * function must preceed the call of init member function. The call of
     * this function must preceed the call of init member function. */
    void set_ht_size(size_t);

    /*! Sets hash function to be used. */
    void set_hash_function(hash_functions_t);   

    /*! Sets initial size of a collision table (default=1). The call of this
     * function must preceed the call of init member function. */
    void set_col_init_size(size_t);
    
    /*! Sets the factor by which a collision table is enlarged if necessary.
     * The call of this function must preceed the call of init member
     * function. */
    void set_col_resize(size_t);

    /*! Sets a limit for the number of allocated bytes (not including memory
     * fragmentation). The call of this function must preceed the call of
     * init member function. */
    bool set_mem_limit(size_t);

    /*! Sets the size of instance of an appendix structure. The call of this function must
     * preceed the call of init member function. */
    void set_appendix_size(size_t);       

    /*! Sets the size of appendix using an instance of an appendix
     *  strucute. Must be called before member function init. */
    template <class appendix_t>
    void set_appendix(appendix_t)
    {
      if (initialized)
	{
	  errvec<<"storage: you cannot call set function after init"
		<<thr(EXPLICIT_STORAGE_ERR_TYPE);
	}
      appendix_size = sizeof(appendix_t);
    }

    /*! Returns pointer to appendix stored at the referenced state. */
    void* app_by_ref(state_ref_t refer);

    /*! Returns appendix stored at the referenced state. */
    template <class appendix_t>
    bool get_app_by_ref(state_ref_t refer, appendix_t& result)
    {
//        std::cout <<"  Storage:.get_app_by_ref ..."<<endl;
      if (appendix_size==0)
	{
	  errvec <<"No appendix set. Cannot use get_app_by_ref()"
		 <<psh(EXPLICIT_STORAGE_ERR_TYPE);
	  return false;
	}

      if (!(storage.ht_base[refer.hres].col_table))
	{
	  errvec <<"Invalid reference used in get_app_by_ref()."
		 <<psh(EXPLICIT_STORAGE_ERR_TYPE);
	  return false;
	}
      else
	{
	  if (!(storage.ht_base[refer.hres].col_table[refer.id].ptr))
	    {
	      errvec <<"Invalid reference used in get_app_by_ref()."
		     <<psh(EXPLICIT_STORAGE_ERR_TYPE);
	      return false;
	    }
	  else
	    {
	      memcpy(&result,
		     storage.ht_base[refer.hres].col_table[refer.id].ptr +
		     storage.ht_base[refer.hres].col_table[refer.id].size,
		     sizeof(appendix_t));
	    }
	}    
//        std::cout <<"  Storage:.get_app_by_ref ... done"<<endl;
      return true;
    }    

    /*! Rewrites the appendix stored at referenced state with the given one. */
    template <class appendix_t>
    bool set_app_by_ref(state_ref_t refer, appendix_t appen)
    {
//        std::cout <<"  Storage:.set_app_by_ref ... "<<endl;
      if (appendix_size==0)
	{
	  errvec <<"No appendix set. Cannot use set_app_by_ref()"
		 <<psh(EXPLICIT_STORAGE_ERR_TYPE);
	  return false;
	}
      
      if (!(storage.ht_base[refer.hres].col_table))
	{
	  errvec <<"Invalid reference used in set_app_by_ref()."
		 <<psh(EXPLICIT_STORAGE_ERR_TYPE);
	  return false;
	}
      else
	{
	  if (!(storage.ht_base[refer.hres].col_table[refer.id].ptr))
	    {
	      errvec <<"Invalid reference used in set_app_by_ref()."
		     <<psh(EXPLICIT_STORAGE_ERR_TYPE);
	      return false;
	    }
	  else
	    {
//  	      std::cout <<"    - size:"<<storage.ht_base[refer.hres].col_table[refer.id].size<<endl;
	      memcpy(storage.ht_base[refer.hres].col_table[refer.id].ptr +
		     storage.ht_base[refer.hres].col_table[refer.id].size,
		     &appen,
		     sizeof(appendix_t));
	    }
	}    
//        std::cout <<"  Storage:.get_app_by_ref ... done"<<endl;
      return true;
    }

  protected:
    typedef struct {
      char *ptr;
      int size;
    } col_member_t;
    
    typedef struct {
      size_t col_size;
      col_member_t *col_table;
    } ht_member_t;
    
    typedef struct {
      ht_member_t *ht_base;
      size_t states_stored;
      size_t states_max_stored;
      size_t states_col;      // sum of sizes of collision tables
      size_t states_max_col;  // maximal size of a collision table
      size_t ht_occupancy;
      size_t max_ht_occupancy;
      size_t collisions;
      size_t max_collisions;
    } storage_t;

    storage_t storage;

    bool initialized;

    size_t compression_method;
    compressor_t compressor;

    size_t ht_size;
    size_t col_init_size;
    size_t col_resize;
    
    size_t appendix_size;
    
    error_vector_t& errvec;
    
    size_t mem_limit;
    size_t mem_used;
    size_t mem_max_used;
    
    void mem_counting(int);
    hash_function_t hasher;
    
    //size_t hash_function(state_t);
    
  };
#ifndef DOXYGEN_PROCESSING
}
#endif //end of namespace divine which shouldn't be seen by Doxygen
#endif






















#ifndef DIVINE_HASH_FUNCTION_HH
#define DIVINE_HASH_FUNCTION_HH
/*!\file hash_function.hh
 * This header file contains the definition of class hash_function_t.
 * The class is instantiated by explicit_storage_t and distributed_t instances.
 * 
 * \author Jiri Barnat
 */

#ifndef DOXYGEN_PROCESSING
#include "common/error.hh"
#include "common/types.hh"
#include "system/state.hh"

namespace divine { //We want Doxygen not to see namespace `divine'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING


  //!Type to identify hash functions.
  enum hash_functions_t {DEFAULT, JENKINS, DIVINE, HC4};

  //!Class that unifies hash functions in the library 
  class hash_function_t {
  protected:
    hash_functions_t hf_id;
  public:
    //!Constructor
    hash_function_t (hash_functions_t);
    //!Constructor
    hash_function_t ();
    //!Returns hash key for a given state. An optional parameter specify a seed or initvalue for the hash function. 
    size_int_t hash_state(state_t, size_int_t = 1);
    //!Returns hash key for a given piece of memory. An optional parameter specify a seed or initvalue for the hash function.
    size_int_t get_hash(unsigned char *, size_int_t, size_int_t = 1);
    //!Sets hash function to be used for hashing
    void set_hash_function(hash_functions_t);
    //!Destructor
    ~hash_function_t();
  };

#ifndef DOXYGEN_PROCESSING
} //end of namespace

#endif //DOXYGEN_PROCESSING

#endif


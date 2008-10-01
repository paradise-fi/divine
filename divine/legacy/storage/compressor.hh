#ifndef _DIVINE_COMPRESSOR_HH_
#define _DIVINE_COMPRESSOR_HH_

/*!\file
 * Unit for compression of explicit states of system. Implemented by
 * class compressor_t.
 *
 * \author Jiri Barnat
 */

#include "system/state.hh"
#include "system/explicit_system.hh"

#define NO_COMPRESS 11
#define HUFFMAN_COMPRESS 12

namespace divine {

//!Compression implementation class
class compressor_t {
public:
  //! Compress a state according current compression method. 
  /*! Returns where the state is compressed and how long the compressed
   *  representation is. */
  bool compress(state_t, char *& pointer, int& size);
  
  //! Decompress the state using current compression method
  /*! Decompress the state from the given pointer and size using current
   *  compression method. */
  bool decompress(state_t&, char *pointer, int size);
  
  //! Initializes compressor instance.
  /*! Requires identification of
   *  compression method, extra space that should be allocated at each
   *  compressed state (appendix) and pointer to explicit_system_t. */
  bool init(int method, int appendix_size, explicit_system_t& system_pointer);

  //! Clears data structures initialized inside compressor
  /*! Dealocates data that are created during initialization of compressor. This
   *  is needed especially for clearing the arena of compressor. */
  void clear();

private:
  int method_id;
  int oversize;
};

};

#endif



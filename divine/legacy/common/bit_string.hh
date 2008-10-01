#include <cstdlib>
#include <cstring>

#ifndef BIT_STRING_HH
#define BIT_STRING_HH

/*!\file
 * Unit implementing a simple vector of bits using class bit_string_t
 *
 * \author Pavel Simecek
 */

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include "common/deb.hh"
#include "common/types.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

static const ulong_int_t bit_values[32] =
   {1, 2, 4, 8, 16, 32, 64, 128, 256, 1<<9, 1<<10, 1<<11, 1<<12, 1<<13, 1<<14,
    1<<15, 1<<16, 1<<17, 1<<18, 1<<19, 1<<20, 1<<21, 1<<22, 1<<23, 1<<24,
    1<<25, 1<<26, 1<<27, 1<<28, 1<<29, 1<<30, 1<<31};


//!Class for impementation of field of bits
class bit_string_t
{
 private:
 typedef ulong_int_t BIT32_TYPE_t;
 typedef BIT32_TYPE_t* P_BIT32_TYPE_t;
 typedef void* P_VOID;
 P_BIT32_TYPE_t mem;
 size_int_t allocated;
 size_int_t bits;
 static const size_int_t BIT32_SIZE = sizeof(BIT32_TYPE_t);

 public:
 //!A copy constructor
 bit_string_t(const bit_string_t & bit_str2)
  {
   mem = 0;
   alloc_mem(bit_str2.bits);
   memcpy(mem,bit_str2.mem,allocated*BIT32_SIZE);
  }
 //!Copies a content of right side to the bit_string_t instance on the left side
 bit_string_t & operator=(const bit_string_t & bit_str2)
  {
   if (mem) free(mem);
   mem = 0;
   alloc_mem(bit_str2.bits);
   memcpy(mem,bit_str2.mem,allocated*BIT32_SIZE);
   return (*this);
  }
 //!A default constructor
 bit_string_t() { mem = 0; allocated=0; bits=0; }
 //!A constructor allocating space for `bit_count' bits
 /*!This is a simple contructor, that automaticaly allocates a memory for
  * number of bits given in \a bit_count
  *
  * All bits are initially 0.*/
 bit_string_t(const size_int_t bit_count)
  { mem = 0; alloc_mem(bit_count); }
 //!Allocation/realocation method
 /*!This method can be used for initial allocation of memory space (given
  * in count of bits to be allocated - parameter \a bitcount).
  *
  * All bits are initially 0.
  */
 void alloc_mem(const size_int_t bit_count)
  {
   if (mem) free(mem);
   bits = bit_count;
   allocated = bit_count>>5;
   if (allocated<<5 != bit_count) ++allocated;
   DEB(cerr << "Allocating "<< allocated <<" 4bytes"<<endl);
   mem = P_BIT32_TYPE_t(calloc(allocated,BIT32_SIZE));
  }
 //!Returns a memory containing array of bits
 byte_t * get_mem()
  { return (reinterpret_cast<byte_t *>(mem)); }

 //!Returns a size of allocated memory in bytes
 size_int_t get_mem_size() { return (allocated*BIT32_SIZE); }
 //!Sets all bits to 0
 /*!\warning Do not use this function, if no bits are allocated before!*/
 void clear()
  { memset(mem,0,allocated*BIT32_SIZE); }
 //!A destructor.
 ~bit_string_t() { free(P_VOID(mem)); }
 //!Sets `i'-th bit to 1
 void enable_bit(const size_int_t i)
  { mem[i>>5] |= bit_values[i%32]; }
 //!Sets `i'-th bit to 0
 void disable_bit(const size_int_t i)
  { mem[i>>5] &= ~ bit_values[i%32]; }
 //!Sets `i'-th bit to `value'
 void set_bit(const size_int_t i, const bool value)
  { mem[i>>5] = value ? (mem[i>>5]| bit_values[i%32]) :
                        (mem[i>>5]&~bit_values[i%32]); }
 //!Inverts `i'-th bit
 void invert_bit(const size_int_t i)
  { mem[i>>5] ^= bit_values[i%32]; }
 //!Returns a value of `i'-th bit
 bool get_bit(const size_int_t i) const
  {
   BIT32_TYPE_t and_val = bit_values[i%32];
   return (mem[i>>5] & and_val);
  }
 //!Return a count of 4-byte items allocated
 /*!Storage in this class is implemented using allocation of field
  * of 4-byte variables in a memory. This method returns a count of these
  * variables.
  */
 size_int_t get_allocated_4bytes_count() const
  { return allocated; }
 
 //!Returns a count of allocated bits
 size_int_t get_bit_count() const
  { return bits; }
 
 //!Prints a sequence of zeros and ones representing a content
 //! to output stream `outs'.
 void DBG_print(std::ostream & outs = cerr) const
   {
    DEB(cerr << "First 4Byte value:" << (*mem) << endl;)
    DEB(cerr << "Print of bit_string_t: ";)
    for (size_int_t i = 0; i!=bits; ++i)
      if (get_bit(i)) outs << '1'; else outs << '0';
    outs << endl;
   }
 
 //!Bitwisely adds `from' to (*this)
 void add(const bit_string_t & from)
  {
   for (size_int_t i = 0; i!= allocated; ++i)
     mem[i] = mem[i] | from.mem[i];
  }
 
 //!Returns true iff (`bs1' & `bs2') != 0
 friend bool operator&(const bit_string_t & bs1, const bit_string_t & bs2)
  {
   for (size_int_t i = 0; i!= bs1.allocated; ++i)
     if (bs1.mem[i] & bs2.mem[i]) return false;
   return true;
  }
 
 //!Returns true iff (`bs1' | `bs2') != 0
 friend bool operator|(const bit_string_t & bs1, const bit_string_t & bs2)
  {
   for (size_int_t i = 0; i!= bs1.allocated; ++i)
     if (bs1.mem[i] | bs2.mem[i]) return false;
   return true;
  }
 
 //!Returns true iff (`bs1' xor `bs2') != 0
 friend bool operator^(const bit_string_t & bs1, const bit_string_t & bs2)
  {
   for (size_int_t i = 0; i!= bs1.allocated; ++i)
     if (bs1.mem[i] ^ bs2.mem[i]) return false;
   return true;
  }
};

};

#include "common/undeb.hh"

#endif

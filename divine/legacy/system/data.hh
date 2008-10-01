/*!\file
 * This file contains a definition of the class data_t - the general container
 * of data computed in \sys or \exp_sys */

#ifndef DIVINE_DATA_HH
#define DIVINE_DATA_HH

#ifndef DOXYGEN_PROCESSING
#include "common/types.hh"
    
namespace divine { // We don't want Doxygen to see namespace 'dve'
#endif // DOXYGEN_PROCESSING

const ushort_int_t DATA_TYPE_UNKNOWN=MAX_USHORT_INT;
const ushort_int_t DATA_TYPE_SBYTE=0;
const ushort_int_t DATA_TYPE_UBYTE=1;
const ushort_int_t DATA_TYPE_BYTE =1;
const ushort_int_t DATA_TYPE_SSHORT_INT=2;
const ushort_int_t DATA_TYPE_USHORT_INT=3;
const ushort_int_t DATA_TYPE_SLONG_INT=4;
const ushort_int_t DATA_TYPE_ULONG_INT=5;
const ushort_int_t DATA_TYPE_SIZE_INT=6;

//!Class representing a general data type
class data_t
{
 private:
 byte_t * mem;
 size_int_t size;
 size_int_t data_type;

 public:
 data_t() { mem=new byte_t[4]; size=4; data_type = DATA_TYPE_SLONG_INT; }
 ~data_t() { delete [] mem; }
 template <class T> void assign(const T & value)
 {
  if (size!=sizeof(T)) { delete [] mem; mem = new byte_t[sizeof(T)]; }
  memcpy(mem,&value,sizeof(T));
 } 
};

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif



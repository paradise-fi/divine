#include <string>
#include "storage/compressor.hh"
#include "system/dve/dve_explicit_system.hh"
#include "huffman.cci"


bool compressor_t::init(int method_id_in, int oversize_in, explicit_system_t& sys_in)
{
  method_id = method_id_in;
  oversize = oversize_in;

  switch (method_id) {
  case NO_COMPRESS: // no compression
    // no initialization needed;
    break;
  case HUFFMAN_COMPRESS: // Huffman
    huffman_init(2*65536);
    break;
  default:
    gerr<<"Uknown compression method."<<thr();
    return false;
    break;
  }
  return true;
}

void compressor_t::clear()
{
  huffman_clear();
  method_id = 0;
  oversize = 0;
}

// arguments are: state, result, size of compressed state
bool compressor_t::compress(state_t arg1,char *&arg2,int& arg3)
{
  switch (method_id) {
  case NO_COMPRESS: // no compression
    arg2 = new char[arg1.size + oversize];
    arg3 = arg1.size;
    memcpy (arg2,arg1.ptr,arg1.size);
    break;
  case HUFFMAN_COMPRESS: // Huffman
    int tmp_int;
    arg2=huffman_compress(arg1, tmp_int, oversize);
    arg3=tmp_int;
    break;
  default:
    gerr<<"Uknown compression method."<<thr();
    return false;
    break;
  }

//    cout <<"Compress:";
//    for (unsigned int x=0; x<arg1.size; x++)
//      cout <<(int)*((unsigned char *)(arg1.ptr+x))<<",";
//    cout <<endl<<"to:";
//    for (int x=0; x<arg3; x++)
//      cout <<(int)*((unsigned char *)(arg2+x))<<",";
//    cout <<endl;
   
  return true;
}

bool compressor_t::decompress(state_t &state, char* memptr, int statesize)
{

  switch (method_id) {
  case NO_COMPRESS:

    //if (arg1.ptr != NULL) error("arg1.ptr not null");

    state=new_state(statesize);
    memcpy (state.ptr, memptr, statesize);
    break;
  case HUFFMAN_COMPRESS:
//      cout<<"   Calling hufmann-decompress:"<<endl;
    huffman_decompress(state, memptr, statesize, oversize);
    break;
  default:
    gerr<<"Uknown compression method."<<thr();
    return false;
    break;
  }

//    cout <<"Decompress:";
//    for (int x=0; x<statesize; x++)
//      cout <<(int)*((unsigned char *)(memptr+x))<<",";
//    cout <<endl<<"to:";
//    for (unsigned int x=0; x<state.size; x++)
//      cout <<(int)*((unsigned char *)(state.ptr+x))<<",";
//    cout <<endl;

  return true;
}

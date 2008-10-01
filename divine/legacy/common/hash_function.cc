#include <iostream>
#include "common/hash_function.hh"

#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8);  \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12); \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5);  \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}


using namespace divine;

hash_function_t::hash_function_t (hash_functions_t hf)
{
  hf_id = hf;
}

hash_function_t::hash_function_t ()
{
  hf_id = JENKINS;
}

hash_function_t::~hash_function_t ()
{
}

size_int_t
hash_function_t::hash_state(state_t state, size_int_t seed)
{
  return get_hash (reinterpret_cast<unsigned char*>(state.ptr), state.size, seed);
}

size_int_t
hamming_distance(unsigned char*ptr, size_int_t length, bool set_to_zero)
{
  size_int_t dist=0;
  for (size_int_t i=0;i<length; i++)
    {
      size_int_t auxdist=0; 
      /* We suppose char is a byte (8 bits), this is of course not guarented by the standard! */
      auxdist += 
	( (ptr[i] & 1) >0 ? 1 : 0 )   + 
	( (ptr[i] & 2) >0 ? 0 : 1 )   + 
	( (ptr[i] & 4) >0 ? 1 : 0 )   + 
	( (ptr[i] & 8) >0 ? 0 : 1 )   + 
	( (ptr[i] & 16) >0 ? 1 : 0 )  + 
	( (ptr[i] & 32) >0 ? 0 : 1 )  + 
	( (ptr[i] & 64) >0 ? 1 : 0 )  + 
	( (ptr[i] & 128) >0 ? 0 : 1 ) ;
      if (set_to_zero)
	{
	  dist += auxdist;
	}
      else
	{
	  dist += 8-auxdist;
	}
//       std::cout <<"Byte="<<(int)ptr[i]<<" auxdist="<<auxdist<<" dist="<<dist<<endl;
    }
  return dist;
}


size_int_t 
hash_function_t::get_hash(unsigned char *ptr, size_int_t length, size_int_t initval)
{  
  register size_int_t a,b,c,len;
  c=initval;
  a = b = 0x9e3779b9;
  len = length;

  if (!ptr)
    return 0;

  switch (hf_id) {
  case DEFAULT:
  case JENKINS:
    /*---------------------------------------- handle most of the key */
    while (len >= 12)
      {
        a += (ptr[0]+((size_int_t)ptr[1]<<8) +((size_int_t)ptr[2]<<16) +((size_int_t)ptr[3]<<24));
        b += (ptr[4]+((size_int_t)ptr[5]<<8) +((size_int_t)ptr[6]<<16) +((size_int_t)ptr[7]<<24));
        c += (ptr[8]+((size_int_t)ptr[9]<<8) +((size_int_t)ptr[10]<<16)+((size_int_t)ptr[11]<<24));
        mix(a,b,c);
        ptr += 12; len -= 12;
      }

    /*------------------------------------- handle the last 11 bytes */
    c += length;
    switch(len)  
      {
      case 11: c+=((size_int_t)ptr[10]<<24);
      case 10: c+=((size_int_t)ptr[9]<<16);
      case 9 : c+=((size_int_t)ptr[8]<<8);
        /* the first byte of c is reserved for the length */
      case 8 : b+=((size_int_t)ptr[7]<<24);
      case 7 : b+=((size_int_t)ptr[6]<<16);
      case 6 : b+=((size_int_t)ptr[5]<<8);
      case 5 : b+=ptr[4];
      case 4 : a+=((size_int_t)ptr[3]<<24);
      case 3 : a+=((size_int_t)ptr[2]<<16);
      case 2 : a+=((size_int_t)ptr[1]<<8);
      case 1 : a+=ptr[0];
      }
    mix(a,b,c);
    return c;
    break;

  case DIVINE:
    for(a=0;a<length;a++)
      {
        if (*ptr==0)
          {
            c *= 99277;
          }
        else
          {
            c += 104729 ^ (*ptr) ;
            c *= 57193;
          }
        ptr++;
      }
    return c;
    break;
  case HC4:
    a=0; // min achieved value
    b=0; // where was min achieved
    for (c=1; c<=4; c++)
      {
	switch(c)
	  {
	  case 1:
	    len=hamming_distance(&(ptr[0]),length/2,true);
	    break;	    
	  case 2:
	    len=hamming_distance(&(ptr[0]),length/2,false);
	    break;	    
	  case 3:
	    len=hamming_distance(&(ptr[length/2]),length-(length/2),true);
	    break;	    
	  case 4:
	    len=hamming_distance(&(ptr[length/2]),length-(length/2),false);
	    break;	    
	  }
// 	std::cout<<"L="<<len<<endl;
	if (len<a || b==0)
	  {
	    a=len;
	    b=c;
	  }	
      }
    return (b-1);
    break;
  }    
  return 0;
}


void
hash_function_t::set_hash_function(hash_functions_t id)
{
  hf_id = id;
  if (hf_id == DEFAULT)
    {
      hf_id = JENKINS;
    }
}













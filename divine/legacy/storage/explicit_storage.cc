#include "storage/explicit_storage.hh"
#include <string>
#include <sstream>

using namespace divine;

// {{{ state_ref_t 

state_ref_t::state_ref_t()
{
  invalidate();
}

void state_ref_t::invalidate()
{
  //page_id = id = hres = 1024*1024+1; //1 048 577
  id = hres = 1024*1024+1; //1 048 577
}

bool state_ref_t::is_valid()
{
  //  if (page_id != id || id != hres || hres != 1024*1024+1)
  if (id != hres || hres != 1024*1024+1)
    return true;
  else 
    return false;
}

std::string state_ref_t::to_string()
{
  if (id != hres || hres != 1024*1024+1)
  {
    std::ostringstream result;
    result <<"(" << hres <<","<< id <<")";
    return result.str();
  }
  else
  {
    return "(invalid)";
  }
}
// }}}

// {{{ constructor 

explicit_storage_t::explicit_storage_t(error_vector_t& errorvector):errvec(errorvector)
{
  initialized = false;

  compression_method = NO_COMPRESS;

  ht_size = 65536 * 64;  // 2^22
  col_init_size = 1;
  col_resize = 1;

  appendix_size = 0;

  mem_limit=0;   //no limit
  mem_used=0;
  mem_max_used=0;

}

// }}}
// {{{ init 

void explicit_storage_t::init(explicit_system_t& sys)
{
  initialized = true;

  memset (&storage,0,sizeof(storage_t));

  mem_counting(ht_size * sizeof(ht_member_t));      
  storage.ht_base = new ht_member_t[ht_size];
  memset (storage.ht_base,0, ht_size * sizeof(ht_member_t));

  compressor.clear();
  compressor.init(compression_method, appendix_size, sys);
}

// }}}
// {{{ set_functions 

void explicit_storage_t::set_col_init_size(size_t coltable_init_size)
{
  if (initialized)
    {
      errvec<<"storage: you cannot call set function after init"
	    <<thr(EXPLICIT_STORAGE_ERR_TYPE);
    }
  if (coltable_init_size==0)
    {
      errvec<<"storage: cannot initialize colision table size to 0"
	    <<thr(EXPLICIT_STORAGE_ERR_TYPE);
    }    
  col_init_size = coltable_init_size;
}

void explicit_storage_t::set_ht_size(size_t hashtable_size)
{
  if (initialized)
    {
      errvec<<"storage: you cannot call set function after init"
	    <<thr(EXPLICIT_STORAGE_ERR_TYPE);
    }
  ht_size = hashtable_size;
}

void explicit_storage_t::set_compression_method(size_t compression_method_id)
{
  if (initialized)
    {
      errvec<<"storage: you cannot call set function after init"
	    <<thr(EXPLICIT_STORAGE_ERR_TYPE);
    }
  if (
      compression_method_id == NO_COMPRESS ||
      compression_method_id == HUFFMAN_COMPRESS
      )
    {
      compression_method = compression_method_id;
    }
  else
    {      
      errvec<<"storage: unknown compression method"
	    <<thr(EXPLICIT_STORAGE_ERR_TYPE);
    }
}


void explicit_storage_t::set_col_resize(size_t coltable_resize_by)
{
  if (initialized)
    {
      errvec<<"storage: you cannot call set function after init"
	    <<thr(EXPLICIT_STORAGE_ERR_TYPE);
    }
  col_resize = coltable_resize_by;
}

void explicit_storage_t::set_hash_function(hash_functions_t hf)
{
  hasher.set_hash_function(hf);
}

bool explicit_storage_t::set_mem_limit(size_t memory_limit)
{
  if (initialized)
    {
      errvec<<"storage: you cannot call set function after init"
	    <<thr(EXPLICIT_STORAGE_ERR_TYPE);
    }
  if(mem_used <= memory_limit)
    {
      mem_limit = memory_limit;
      return true;
    }
  return false;
}

void explicit_storage_t::set_appendix_size(size_t appendix_size_in)
{
  if (initialized)
    {
      errvec<<"storage: you cannot call set function after init"
	    <<thr(EXPLICIT_STORAGE_ERR_TYPE);
    }
  appendix_size = appendix_size_in;
  return;
}

// }}}
// {{{ get_stats 

size_t explicit_storage_t::get_mem_used()
{
  return mem_used;
}

size_t explicit_storage_t::get_mem_max_used()
{
  return mem_max_used;
}

size_t explicit_storage_t::get_states_stored()
{
  return storage.states_stored;
}

size_t explicit_storage_t::get_states_max_stored()
{
  return storage.states_max_stored;
}

size_t explicit_storage_t::get_coltables()
{
  return storage.states_col;
}

size_t explicit_storage_t::get_max_coltable()
{
  return storage.states_max_col;
}

size_t explicit_storage_t::get_ht_occupancy()
{
  return storage.ht_occupancy;
}

// }}}
// {{{ mem_counting 

void explicit_storage_t::mem_counting(int modify_mem_counter_by)
{
  if ((mem_used + modify_mem_counter_by > mem_limit) && (mem_limit > 0))
    {      
      errvec <<"Memory limit reached."
	     <<thr(EXPLICIT_STORAGE_ERR_TYPE);
    }
  else
    {
      mem_used += modify_mem_counter_by;
      if (mem_max_used < mem_used) 
	{
	  mem_max_used = mem_used;
	}
    }
}

// }}}
// {{{ insert 

void explicit_storage_t::insert (state_t state)
{
  state_ref_t tmp_ref;
  insert(state,tmp_ref);
  return;
}


void explicit_storage_t::insert (state_t state, state_ref_t& state_reference)
{
//    std::cout <<"  Storage.insert ..."<<endl;

  size_t hresult = 0;
  size_t pos = 0;
  bool free_slot = false;
  bool is_a_collision = false;
  col_member_t *tmp_table = 0;
  char *cstate = 0;
  int cstate_size = 0;

  hresult = hasher.get_hash(reinterpret_cast<unsigned char *>(state.ptr),state.size,
			    EXPLICIT_STORAGE_HASH_SEED);  
  hresult = hresult % ht_size;
  
  // compress state  
  if (!compressor.compress
      (state,cstate,cstate_size))
    {
      errvec<<"insert(): Cannot compress the state."
	    <<thr(EXPLICIT_STORAGE_ERR_TYPE);
      return;
    }

  if (!(storage.ht_base[hresult].col_table)) 
    //the first state to be hashed here
    {
      mem_counting(col_init_size * sizeof(col_member_t));
      storage.ht_base[hresult].col_table = new col_member_t[col_init_size];
      memset(storage.ht_base[hresult].col_table,0,sizeof(col_member_t)*col_init_size);
      storage.ht_base[hresult].col_size = col_init_size;
      pos = 0;
      storage.states_col += col_init_size;
      if (storage.states_max_col < col_init_size)
	{
	  storage.states_max_col = col_init_size;
	}
    }
  else  // try to locate a free slot or the state in col_table
    {
      is_a_collision = true;
      for (size_t i=0; i<storage.ht_base[hresult].col_size; i++)
	{
	  if (!(storage.ht_base[hresult].col_table[i].ptr))
	    {
	      if (!free_slot) pos = i;
	      free_slot = true;
	    }
	  else
	    {
	      if (cstate_size == storage.ht_base[hresult].col_table[i].size)
		{
		  if (memcmp (cstate,storage.ht_base[hresult].col_table[i].ptr,
			      storage.ht_base[hresult].col_table[i].size)
		      == 0)
		    {
		      //already inserted ...
		      delete [] cstate;
//  		      std::cout <<"  Storage.insert ... done (inserted before)"<<endl;
		      return;
		    }
		}
	    }
	}
      
      // if a free slot wasn't found then resize col_table
      if (!free_slot)
	{
	  if (col_resize == 0)
	    {
	      errvec<<"Cannot resize collision list (col_resize=0)."
		    <<thr(EXPLICIT_STORAGE_ERR_TYPE);
	    }
	  mem_counting(sizeof(col_member_t) * col_resize);
	  tmp_table = new col_member_t[storage.ht_base[hresult].col_size + col_resize];
	  memset(tmp_table,0,(storage.ht_base[hresult].col_size + col_resize)*sizeof(col_member_t));
	  memcpy (tmp_table, storage.ht_base[hresult].col_table, storage.ht_base[hresult].col_size * sizeof(col_member_t));
	  delete [] storage.ht_base[hresult].col_table;
	  pos = storage.ht_base[hresult].col_size;
	  storage.ht_base[hresult].col_table = tmp_table;
	  storage.ht_base[hresult].col_size += col_resize;
	  storage.states_col += col_resize;
	  if (storage.states_max_col < storage.ht_base[hresult].col_size)
	    {
	      storage.states_max_col = storage.ht_base[hresult].col_size;
	    }
	}
    }      
  
  
  // pos points to first free slot

  mem_counting(cstate_size + appendix_size);     
  storage.ht_base[hresult].col_table[pos].ptr = cstate;
  storage.ht_base[hresult].col_table[pos].size = cstate_size;

  state_reference.hres = hresult;
  state_reference.id = pos;

//    std::cout <<"UUU "<<state_reference.hres<<","<<state_reference.id<<endl;

  storage.states_stored ++;
  if (is_a_collision)
    {
      storage.collisions ++;
      if (storage.max_collisions < storage.collisions)
	{
	  storage.max_collisions = storage.collisions;
	}
    }
  else
    {
      storage.ht_occupancy ++;
      if (storage.max_ht_occupancy < storage.ht_occupancy)
	{
	  storage.max_ht_occupancy = storage.ht_occupancy;
	}
    }

  if (storage.states_max_stored < storage.states_stored)
    {
      storage.states_max_stored = storage.states_stored;
    }
  
//    std::cout <<"  Storage.insert ... done"<<endl;
  return;
}


bool explicit_storage_t::is_stored_if_not_insert(state_t state,state_ref_t& state_reference)
{
//    std::cout <<"  Storage.is_stored_if_not_insert ..."<<endl;
  size_t hresult = 0;
  size_t pos = 0;
  bool free_slot = false;
  bool is_a_collision = false;
  col_member_t *tmp_table = 0;
  char *cstate = 0;
  int cstate_size = 0;
  //int tmp_extra_mem = 0;
  bool already_stored=false;

  hresult = hasher.get_hash(reinterpret_cast<unsigned char *>(state.ptr),state.size,
			    EXPLICIT_STORAGE_HASH_SEED);  
  hresult = hresult % ht_size;
  
  if (!compressor.compress
      (state,cstate,cstate_size))
    {
      errvec<<"is_stored_if-not_insert(): Cannot compress the state."
	    <<thr(EXPLICIT_STORAGE_ERR_TYPE);
      return false;
    }

  if (!(storage.ht_base[hresult].col_table)) 
    //the first state to be hashed here
    {
      mem_counting(col_init_size * sizeof(col_member_t));
      storage.ht_base[hresult].col_table = new col_member_t[col_init_size];
      memset(storage.ht_base[hresult].col_table,0,sizeof(col_member_t)*col_init_size);
      storage.ht_base[hresult].col_size = col_init_size;
      pos = 0;
      storage.states_col += col_init_size;
      if (storage.states_max_col < col_init_size)
	{
	  storage.states_max_col = col_init_size;
	}
    }
  else  // try to locate a free slot or the state in col_table
    {
      is_a_collision = true;
      for (size_t i=0; i<storage.ht_base[hresult].col_size; i++)
	{
	  if (!(storage.ht_base[hresult].col_table[i].ptr))
	    {
	      if (!free_slot) pos = i;
	      free_slot = true;
		}
	  else
	    {
	      if (cstate_size == storage.ht_base[hresult].col_table[i].size)
		{
		  if (memcmp (cstate,storage.ht_base[hresult].col_table[i].ptr,
			      storage.ht_base[hresult].col_table[i].size)
		      == 0)
		    {
		      //already inserted ...
		      already_stored = true;
		      delete [] cstate;
		      state_reference.hres = hresult;
		      state_reference.id = i;
		      return true;
		    }
		}
	    }
	}
 
      // if a free slot wasn't found then resize col_table
      if (!free_slot)
	{
	  if (col_resize == 0)
	    {
	      errvec<<"Cannot resize collision list (col_resize=0)."
		    <<thr(EXPLICIT_STORAGE_ERR_TYPE);
	    }
	  mem_counting(sizeof(col_member_t) * col_resize);
	  tmp_table = new col_member_t[storage.ht_base[hresult].col_size + col_resize];
	  memset(tmp_table,0,(storage.ht_base[hresult].col_size + col_resize)*sizeof(col_member_t));
	  memcpy (tmp_table, storage.ht_base[hresult].col_table, storage.ht_base[hresult].col_size * sizeof(col_member_t));
	  delete [] storage.ht_base[hresult].col_table;
	  pos = storage.ht_base[hresult].col_size;
	  storage.ht_base[hresult].col_table = tmp_table;
	  storage.ht_base[hresult].col_size += col_resize;
	  storage.states_col += col_resize;
	  if (storage.states_max_col < storage.ht_base[hresult].col_size)
	    {
	      storage.states_max_col = storage.ht_base[hresult].col_size;
	    }
	}
    }      
  
  // pos points to first free slot
  mem_counting(cstate_size + appendix_size);
  storage.ht_base[hresult].col_table[pos].ptr = cstate;
  storage.ht_base[hresult].col_table[pos].size = cstate_size;

  state_reference.hres = hresult;
  state_reference.id = pos;

  storage.states_stored ++;
  if (is_a_collision)
    {
      storage.collisions ++;
      if (storage.max_collisions < storage.collisions)
	{
	  storage.max_collisions = storage.collisions;
	}
    }
  else
    {
      storage.ht_occupancy ++;
      if (storage.max_ht_occupancy < storage.ht_occupancy)
	{
	  storage.max_ht_occupancy = storage.ht_occupancy;
	}
    }

  if (storage.states_max_stored < storage.states_stored)
    {
      storage.states_max_stored = storage.states_stored;
    }
  return false;
}

// }}}
// {{{ is_stored 

bool explicit_storage_t::is_stored (state_t state)
{
  state_ref_t tmp_ref;
  return is_stored(state,tmp_ref);
}

bool explicit_storage_t::is_stored (state_t state, state_ref_t& state_reference)
{
//    std::cout <<"  Storage.is_stored ... "<<endl;
  size_t hresult = 0;

  char *cstate = 0;
  int cstate_size = 0;
  
  hresult = hasher.get_hash(reinterpret_cast<unsigned char *>(state.ptr),state.size,
			    EXPLICIT_STORAGE_HASH_SEED);  
  hresult = hresult % ht_size;
  
  if (!compressor.compress(state,cstate,cstate_size))
    {
//        std::cout <<"  Storage.is_stored ... done (false)"<<endl;
      return false;
    }

  if (!(storage.ht_base[hresult].col_table)) 
    {
      delete [] cstate;
//        std::cout <<"  Storage.is_stored ... done (false)"<<endl;
      return false;
    }
  for (size_t i=0; i<storage.ht_base[hresult].col_size; i++)
    {
      if (cstate_size 
	  == storage.ht_base[hresult].col_table[i].size && 
	  storage.ht_base[hresult].col_table[i].ptr) 
	{
	  if (memcmp (cstate,
		      storage.ht_base[hresult].col_table[i].ptr,
		      cstate_size)
	      == 0)
	    {
	      state_reference.hres = hresult;
	      state_reference.id = i;
	      delete [] cstate;
//  	      std::cout <<"  Storage.is_stored ... done (true)"<<endl;
	      return true;
	    }
	}
    }
  delete [] cstate;
//    std::cout <<"  Storage.is_stored ... done (false)"<<endl;
  return false;
}

// }}}
// {{{ delete_by_ref 

bool explicit_storage_t::delete_by_ref (state_ref_t state_reference)
{
  
  if (!(storage.ht_base[state_reference.hres].col_table))
    {
      errvec <<"delet_by_ref(): invalid reference ..."
	     <<psh(EXPLICIT_STORAGE_ERR_TYPE);
      return false;
    }
  else
    {
      if (!(storage.ht_base[state_reference.hres].col_table[state_reference.id].ptr))
	{
	  errvec <<"delete_by_ref(): invalid reference ..."
		 <<psh(EXPLICIT_STORAGE_ERR_TYPE);
	  return false;
	}
      else
	{
	  mem_counting(-(storage.ht_base[state_reference.hres].col_table[state_reference.id].size));
	  mem_counting(-appendix_size);
	  delete [] (storage.ht_base[state_reference.hres].col_table[state_reference.id].ptr);
	  storage.ht_base[state_reference.hres].col_table[state_reference.id].ptr = 0;
	  storage.ht_base[state_reference.hres].col_table[state_reference.id].size = 0;
	}
    }    
  
  storage.states_stored--;
  return true;	  
}

// }}}
// {{{ delete_all_states 
void explicit_storage_t::delete_all_states(bool leave_collision_lists)
{
  size_t tmp_col_size;
  
  for (size_t ht_row=0; ht_row<ht_size; ht_row++)
    {
      tmp_col_size = storage.ht_base[ht_row].col_size;	  
      /* if tmp_col_size == 0 then the following loop is skipped */
      for (size_t pos_in_row=0; pos_in_row<tmp_col_size; pos_in_row++)
	{
	  if ((storage.ht_base[ht_row].col_table[pos_in_row].ptr)!=0)
	    {
	      mem_counting(-(storage.ht_base[ht_row].col_table[pos_in_row].size));
	      mem_counting(-appendix_size);
	      delete [] (storage.ht_base[ht_row].col_table[pos_in_row].ptr);
	      storage.ht_base[ht_row].col_table[pos_in_row].ptr = 0;
	      storage.ht_base[ht_row].col_table[pos_in_row].size = 0;
	    }
	}
      if (!leave_collision_lists && tmp_col_size>0)
	{
	  mem_counting(-(sizeof(col_member_t)*tmp_col_size));
	  delete [] (storage.ht_base[ht_row].col_table);
	  storage.ht_base[ht_row].col_table = NULL;
	  storage.ht_base[ht_row].col_size = 0;
	}
      
    }
  
  if (!leave_collision_lists)
    {
      storage.collisions=0;
      storage.states_col=0;
    }
  
  storage.states_stored = 0;
  storage.collisions=0;
  storage.ht_occupancy=0;
}
// }}}
// {{{ reconstruct 

state_t explicit_storage_t::reconstruct(state_ref_t state_reference)
{


//    std::cout <<"  Storage.reconstruct ... "<<state_reference.to_string()<<endl;
  state_t result;
  result.ptr = 0;
  result.size = 0;

  if (storage.ht_base[state_reference.hres].col_table)
    {
      if (storage.ht_base[state_reference.hres].col_table[state_reference.id].ptr)
	{	      
	  compressor.decompress(result,
				storage.ht_base[state_reference.hres].col_table[state_reference.id].ptr,
				storage.ht_base[state_reference.hres].col_table[state_reference.id].size
				);

//  	  std::cout <<"  Storage.reconstruct ... done "<<endl;
	  return result;
	}
    }    
  
  errvec <<"reconstruct(): invalid reference ..."
	 <<psh(EXPLICIT_STORAGE_ERR_TYPE);
  return result;
}

// }}}
// {{{ appendix 

void* explicit_storage_t::app_by_ref(state_ref_t refer)
{
  if (appendix_size==0)
    {
      errvec <<"No appendix set. Cannot use app_by_ref()"
	     <<psh(EXPLICIT_STORAGE_ERR_TYPE);
      return 0;
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
	  return
	    (storage.ht_base[refer.hres].col_table[refer.id].ptr +
	     storage.ht_base[refer.hres].col_table[refer.id].size);
	}
    }    
}

// }}}




























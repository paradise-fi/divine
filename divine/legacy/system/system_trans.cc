#include "system/system_trans.hh"
#include "system/explicit_system.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif


//enabled_trans_t * divine::system_new_field_of_objects(const void * params, const size_int_t count)
//{
// explicit_system_t * p_explicit_system = (explicit_system_t *)(params);
// if (p_explicit_system)
//  { enabled_trans_t * return_value =
//      p_explicit_system->new_field_of_enabled_trans(count);
//    return return_value; }
// else return 0;
//}

enabled_trans_t * divine::system_new_enabled_trans(const void * params)
{
 explicit_system_t * p_explicit_system = (explicit_system_t *)(params);
 if (p_explicit_system)
  { enabled_trans_t * return_value = p_explicit_system->new_enabled_trans();
    return return_value; }
 else return 0;
}


enabled_trans_container_t::enabled_trans_container_t(
                                            const explicit_system_t & system):
  array_of_abstract_t<enabled_trans_t, system_new_enabled_trans>
      (&system,system.get_preallocation_count(),2)
{
 trans_bounds = new size_int_t[system.get_process_count()+1];
 trans_bounds[0]=0;
}

enabled_trans_container_t::~enabled_trans_container_t()
{
 if (trans_bounds) delete [] trans_bounds;
 trans_bounds = 0;
}




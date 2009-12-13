#include "system/dve/dve_process_decomposition.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif

dve_process_decomposition_t::dve_process_decomposition_t(dve_explicit_system_t& system)
    : system(system), initialized(false), dve_proc_ptr(0), 
      proc_id(0), proc_size(0), scc_count(0)
{}

dve_process_decomposition_t::~dve_process_decomposition_t() {
  if (initialized)
    {
  delete[] graf;
  delete[] hran;
  delete[] number;
  delete[] low;
  delete[] zasobnik;
  delete[] on_stack;
  delete[] komp;
  delete[] types;
    }
  initialized = false;
}



void dve_process_decomposition_t::strong(int vrchol) {
  int i, w;    
  pocitadlo++;
  number[vrchol] = pocitadlo;
  low[vrchol] = pocitadlo;
  zasobnik[ps++] = vrchol;
  on_stack[vrchol] = 1;
  for(i=0; i<hran[vrchol]; i++) {
    w = graf[vrchol*h+i];
    if (number[w] == 0) {
  strong(w);
  if (low[vrchol] > low[w]) low[vrchol] = low[w];
    }
    else {
  if (on_stack[w]) {
    if (low[vrchol] > number[w]) low[vrchol] = number[w];
  }
    }
  }
  if (low[vrchol] == number[vrchol]) {
    /*    printf("\nI'm %d Component: ", vrchol);*/
    while (--ps>=0 && number[zasobnik[ps]] >= low[vrchol]) {
  on_stack[zasobnik[ps]] = 0;
  if (zasobnik[ps]>=n) gerr <<"Too many states"<<thr();
  komp[zasobnik[ps]] = low[vrchol];
  /*      printf("%d, ",zasobnik[ps]);       */
    }
    /*    printf("\n");*/
    ps++;
  }
}

void dve_process_decomposition_t::parse_process(std::size_t gid_in)
{
  proc_id = gid_in;
  dve_proc_ptr=dynamic_cast<dve_process_t*>(system.get_process(proc_id));
  proc_size = dve_proc_ptr->get_state_count();
  
  if (initialized)
    {
  delete[] graf;
  delete[] hran;
  delete[] number;
  delete[] low;
  delete[] zasobnik;
  delete[] on_stack;
  delete[] komp;
  delete[] types;
    }
  initialized = true;
  
  //  int MAX = 300;
  
  n = dve_proc_ptr->get_state_count();
  h = dve_proc_ptr->get_trans_count();
  graf = new int[n*h];
  hran = new int[std::max(n, h)];
  pocitadlo = 1;
  number = new int[n];
  low = new int[n];
  zasobnik = new int[n];
  ps = 0; 
  on_stack = new int[n];
  komp = new int[n];
  types = new int[n];
  
  for (int x=0; x<n;x++)
    {
  for (int y=0;y<h;y++) graf[x*h+y]=0;
  hran[x]=0;
  number[x]=0;
  low[x]=0;
  zasobnik[x]=0;
  on_stack[x]=0;
  komp[x]=0;
  types[x]=-1;
    }
  
  int i=0;
  while (i<h)
    {
  const dve_transition_t *tr =
    dynamic_cast<const dve_transition_t*>(dve_proc_ptr->get_transition(i));
  int f=tr->get_state1_lid();
  int t=tr->get_state2_lid();
  //cout <<"storing an edge from "<<f<<" to "<<t<<endl;
  graf[f*h+(hran[f])]=t;
  hran[f]++;
  i++;
    }
  
  // add transitions to an error state
  
  //    for (int x=0; x<n; x++)
  //      {
  //        int e=dve_proc_ptr->get_error_state_id();
  //        if (x!=e)
  //  	{
  //  	  graf[x*h+(hran[x])]=e;
  //  	  hran[x]++;
  //  	}
  //      }

  //    for (int x=0; x<n; x++)
  //      {
  //        cout <<"state "<<x<<" has "<<hran[x]<<" edge(s) leading to ";
  //        for (int y=0; y<hran[x]; y++)
  //  	cout <<graf[x*h+y]<<",";
  //        cout <<"\b "<<endl;
  //      }

  strong(dve_proc_ptr->get_initial_state());

  /* ------------------------------- *
   * --- precislovani komponent  --- *
   * ------------------------------- */

  int max_scc_nmbr=0,found=0,last_done=-1,renmbr=0;
  
  for (int x=0;x<n;x++)
    if (max_scc_nmbr<komp[x])
  max_scc_nmbr=komp[x];
  
  while (last_done!=max_scc_nmbr)
    {
  while(!found)
    {
      last_done++;
      for(int x=0;x<n;x++)
        if (komp[x]==last_done) found=1;
    }
    found=0;
    for (int x=0;x<n;x++)
      {
        if(komp[x]==last_done)
          komp[x]=renmbr;
      }
    renmbr++;
    }

  scc_count = renmbr;
  
  /* ------------------------------- *
   * ---  urceni typu komponent  --- *
   * ------------------------------- */
  
  for(int x=0;x<n;x++)
    {
  int tmp=komp[x];
  bool acc = dve_proc_ptr->get_acceptance(x);
  
  if (acc)
    {
      switch (types[tmp]) {
      case -1:
        types[tmp]=2;
        break;
      case 0:
        types[tmp]=1;
        break;
      case 1:
      case 2:
        break;
      }
    }
  else
      {
        switch (types[tmp]) {
        case -1:
          types[tmp]=0;
          break;
    case 0:
      break;
        case 1:
          break;
        case 2:
          types[tmp]=1;
          break;
        }
      };
    }
}
  
int dve_process_decomposition_t::get_process_scc_id(state_t& state)
{
    size_t localstate = system.get_state_of_process(state, proc_id);
    return komp[localstate];
}

int dve_process_decomposition_t::get_process_scc_type(state_t& state)
{
    size_t localstate = system.get_state_of_process(state, proc_id);
    return types[komp[localstate]];
}

int dve_process_decomposition_t::get_scc_type(int scc)
{
  return types[scc];
}

int dve_process_decomposition_t::get_scc_count()
{
  return scc_count;
}

bool dve_process_decomposition_t::is_weak()
{
  for (int i=0; i<scc_count; i++)
    {
  if (types[i]==1)
    return false;
    }
  return true;
}

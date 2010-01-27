 /* ProbDiVinE - Probabilistic Distributed Verification Environment
  * Copyright (C) 2002-2008  Jiri Barnat, Milan Ceska, Jana Tumova,
  * Faculty of Informatics Masaryk University Brno
  *
  * ProbDiVinE (both programs and libraries included in the distribution) is free
  * software; you can redistribute it and/or modify it under the terms of the
  * GNU General Public License as published by the Free Software Foundation;
  * either version 2 of the License, or (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  * or see http://www.gnu.org/licenses/gpl.txt
  */


#include <getopt.h>
#include <sstream>
#include <iostream>
#include <queue>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <divine/legacy/storage/explicit_storage.hh>
#include <divine/legacy/system/dve/dve_prob_explicit_system.hh>
#include <divine/legacy/common/sysinfo.hh>
//#include <divine/legacy/sevine.h>
#include <divine/version.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <iomanip>

#include <lpsolve/lp_lib.h>

using namespace std;
using namespace divine;


const int cacheLine = 64;
const int WHEN_READ_FROM_BUFFER = 4;
const int WHEN_TERMINATING = 5;

char mode; // mode of runnig
char type; // type of verification

// non-blocking FIFO

struct NoopMutex {
    void lock() {}
    void unlock() {}
};

struct remote_state_t
{
  state_t state;    // for forward reachibility
  state_ref_t ref; // reference to predecessor during the first reachibility after reference to state for backward reachibility
  prob_and_property_trans_t action;
  bool probabilistic;

  unsigned short int tag; // tag of the job (0 = forward reachibily, 1 = backward reachibility, 2 = owcty ), later predecessor root id
  int job; // job id later size of SCC corresponding to predecessor root
};

struct remote_SCC_t
{
  state_ref_t pred_root;
  int pred_root_id;
  state_ref_t root;
  int root_id;
  state_ref_t ref;
  int ref_id;
  int pred_size;
  int size;
};


template< typename T, typename WM, int NodeSize = (4096 - cacheLine - sizeof(int) - sizeof(void*)) / sizeof(T) >
struct Fifo {
protected:
    WM writeMutex;
    // the Node layout puts read and write counters far apart to avoid
    // them sharing a cache line, since they are always written from
    // different threads
    struct Node {
        int read;
        char padding[ cacheLine - sizeof(int) ];
        T buffer[ NodeSize ];
        // TODO? we can save some multiplication instructions by using
        // pointers/iterators here, where we can work with addition
        // instead which is very slightly cheaper
        volatile int write;
        Node *next;
        Node() {
            read = write = 0;
            next = 0;
        }
    };

    // pad the fifo object to ensure that head/tail pointers never
    // share a cache line with anyone else
    char _padding1[cacheLine-2*sizeof(Node*)];
    Node *head, *freetail;
    char _padding2[cacheLine-2*sizeof(Node*)];
    Node *tail, *freehead;
    char _padding3[cacheLine-2*sizeof(Node*)];

public:
    Fifo() {
        head = tail = new Node();
        freehead = freetail = new Node();
        assert( empty() );
    }

    // copying a fifo is not allowed
    Fifo( const Fifo &f ) {
        head = tail = new Node();
        freehead = freetail = new Node();
        assert( empty() );
    }

    virtual ~Fifo() {} // TODO free up stuff here

    void push( const T& x ) {
        Node *t;
        writeMutex.lock();
        if ( tail->write == NodeSize ) {
            if ( freehead != freetail ) {
                // grab a node for recycling
                t = freehead;
                assert( freehead->next != 0 );
                freehead = freehead->next;

                // clear it
                t->read = t->write = 0;
                t->next = 0;

                // dump the rest of the freelist
                while ( freehead != freetail ) {
                    Node *next = freehead->next;
                    assert( next != 0 );
                    delete freehead;
                    freehead = next;
                }

               // assert( freehead == freetail );
            } else {
                t = new Node();
            }
        } else
            t = tail;

        t->buffer[ t->write ] = x;
        ++ t->write;

        if ( tail != t ) {
            tail->next = t;
            tail = t;
        }
        writeMutex.unlock();
    }

    bool empty() {
        return head == tail && head->read >= head->write;
    }

    void dropHead() {
        Node *old = head;
        head = head->next;
        assert( head );
        old->next = 0;
        freetail->next = old;
        freetail = old;
    }

    void pop() {
        assert( !empty() );
        ++ head->read;
        if ( head->read == NodeSize ) {
            if ( head->next != 0 ) {
                dropHead();
            }
        }
    }

    T &front() {
        assert( head );
        assert( !empty() );
        // last pop could have left us with empty queue exactly at an
        // edge of a block, which leaves head->read == NodeSize
        if ( head->read == NodeSize ) {
            dropHead();
        }
        return head->buffer[ head->read ];
    }
};

typedef Fifo<remote_state_t,NoopMutex> MyFifo;

typedef Fifo<remote_SCC_t,NoopMutex> MyFifo_SCC;

timeinfo_t timer,lp_timer,scc_timer;
vminfo_t vm;

int Thread_count = 0; 

bool Finnished = false;
bool SCC_Finnished = false;
bool Solved = false;

int  Working_set = -1;
// locking variable
bool system_locked = false;

struct padding_int_t
{
volatile int value;
char padding[cacheLine - sizeof(int)];
};

vector<padding_int_t> waiting_status; // have to be integers instead of bools because of race condition
vector<padding_int_t> creating_status;
vector<padding_int_t> synchronization_alfaro;
vector<padding_int_t> synchronization_scc;
vector<padding_int_t> synchronization_scc_graph;
vector<padding_int_t> inicialization;

struct info_t
{
 int inset; //  set of the state in which we performe  the reachibility  (0 for first job we work on all state)
 int states; // number of visited states in corresponding job

 unsigned short int tag;
 queue<state_ref_t>* reached; // corresponding to set Reached in the paper
 unsigned int range; // corresponding to set Range in the paper
 int executed_by;  // the forward reachibility which executed this job
 int executed_by_BWD; // the backward reachibility which executed this job
 bool has_root; // has the root ?
 state_ref_t root; // root for forward reachibility necessery for next owcty 
 queue<state_ref_t>* seeds; // corresponding to set Seeds returned by FWDSEED in the paper
 queue<state_ref_t>* SCC_list;
};

struct thread_state_ref_t
{
  state_ref_t state_ref;
  int thread_id; 

 bool operator<(const thread_state_ref_t& S) const
 {
   if (this->state_ref < S.state_ref )
    return true;
   if (this->state_ref > S.state_ref )
    return false;
   if (this->thread_id < S.thread_id )
    return true;
   if (this->thread_id > S.thread_id )
    return false;
   return false;
 }

};


//system variables
vector< vector< queue<state_ref_t>* > > Queues_init;
vector< queue<state_ref_t>* >  accepting_queues;
vector< vector<MyFifo> > Buffers;
vector<MyFifo_SCC> Buffers_SCC;
vector< explicit_storage_t*> Storages;
vector< vector<info_t> > Job_infos_init;
vector< map<int,queue<state_ref_t>* > > Queues;
vector< map<int,info_t> > Job_infos;
vector< queue<thread_state_ref_t>* > LP_queues;

state_ref_t initial_ref;
int initial_visited;

int finnishing_id = 0;
int finnishing_job_started = false;
long double Result;

//creating jobs variables
int tmp_states = 0;
unsigned int new_range = 0;
state_ref_t tmp_ref;
bool started_fwd = false;
bool started_owcty = false;
bool seed_found = false;
int fwd_id = 0;
int owcty_id = 0; 

int last_job = 0;
int terminating_job = 0;
unsigned int max_size = 0;
unsigned int max_SCC_size = 1;

int running_job = 0; //(0 = forward reachibily, 1 = eliminating, 2 = exploring )

//mutex
pthread_mutex_t mutex;


// SCC representation
struct SCC_t
{
state_ref_t root;
int root_id;
int succs;
int size;
bool initial; // is true if it contains initial state
set<thread_state_ref_t>* successors;
set<thread_state_ref_t>* predecessors;
set<thread_state_ref_t>* connections; // state for the objective function
};

//SCC list
vector< map<thread_state_ref_t,SCC_t*>* > SCC_lists;

// Complete SCC list 
map<thread_state_ref_t,SCC_t*> Complete_SCC_list;

//Terminal SCC list
queue<thread_state_ref_t> Terminal_SCC_list;

//SCC pointers
vector<SCC_t*> SCC_pointers;

vector<padding_int_t> has_job; // 0 - no_job 1 - has job 2 - solved job 

//info variables
int Number_of_nontrivial_SCC = 0;
int Number_of_trivial_SCC = 0; 
vector<int> number_of_trivial_SCC;
vector<int> states;
vector<int> transitions;
vector<int> number_of_equations;
vector<int> number_of_triv_equations;
int States = 0, Transitions = 0;
int Number_of_equations = 0;
int Number_of_triv_equations = 0;
int previous_explored_state = 0;
int tmp_reached_state = 0;
int tmp_explored_state = 0;
int tmp_eliminated_state = 0;
int seeds = 0;

struct inicialization_info_t
{
 int id;
 dve_prob_explicit_system_t* sys;
};
vector<inicialization_info_t> inicialization_info;


struct prob_thread_state_ref_t
{
  thread_state_ref_t thread_state_ref;
  prob_and_property_trans_t action; 
};



struct appendix_t 
{
  int visited;  //visited in the correponding job 
  int inset;    // membership to the corresponding job
  int actions;  //  number of actions, later size of SCC of corresponding root
  bool is_seed;
  vector<thread_state_ref_t> *predecessors;  //vector of nonprobabilistic predecessors later all predecessors
  vector<prob_thread_state_ref_t> *prob_predecessors; //array of probabilistic predecessors
  vector<prob_and_property_trans_t> *actions_removed; //vector of actions removed - necessary to recognize wheter to decrease number of actions or not

  state_ref_t root;
  int root_id;
  long double value;
};

struct root_state_t
{
   state_ref_t ref;
   bool expanded;
   int id;
};

// Default hash seed for partition function.
const int HASH_SEED = 0xbafbaf99;

hash_function_t hasher;

int partition_function(state_t &state)
{
  return (hasher.get_hash(reinterpret_cast<unsigned char*>(state.ptr),state.size, HASH_SEED) % Thread_count);
}


void *thread_function( void *ptr )
{

  inicialization_info_t* inicialization_info;
  inicialization_info = (inicialization_info_t*) ptr;
  int id = inicialization_info->id;

  //inicialization of info variables
   number_of_trivial_SCC[id] = 0;
   states[id] = 0;
   transitions[id] = 0;
   number_of_equations[id] = 0;
   number_of_triv_equations[id] = 0;

  //terminating and controlling variables
  bool some_work = false;
  bool buffer_readed = false;
  bool waiting_signal = false;
  bool all_waiting = false;
  int  n_cycles = 0;

  //system variables
  state_t s;
  state_ref_t ref,my_tmp_ref;
  appendix_t tmp_appendix;
  state_ref_t pred_ref; //predecessors ref
  thread_state_ref_t thread_state_ref;
  dve_prob_explicit_system_t* sys = inicialization_info->sys;

  /* initialization ... */
  if(mode == 'd')
  {
    cout << "Thread " << id <<" ready"<<endl;
    cout<<"---------------------------------------------------"<<endl;
  }
  appendix_t appendix,pred_appendix;

  Storages[id]->set_appendix(appendix);
  Storages[id]->init(*sys);
  prob_succ_container_t succs_cont;
  prob_succ_container_t::iterator iter_i,iter_j;
  MyFifo* buffer; // pointer to Buffers[id][?]
  MyFifo_SCC* buffer_SCC; // pointer to Buffers[?]


  accepting_queues[id] = new queue<state_ref_t>();

  queue<state_ref_t>* queue_pointer;

  info_t info;

  vector<thread_state_ref_t>::iterator pred_iter;
  vector<prob_thread_state_ref_t>::iterator prob_pred_iter;

 //SCC_lists inicialization
 SCC_lists[id] = new map<thread_state_ref_t,SCC_t*>();

 //inicialization of all queus
 for(int i=0; i<=5; i++)
 {
   Queues_init[id][i] = new queue<state_ref_t>();
 }

 LP_queues[id] = new queue<thread_state_ref_t>();

 has_job[id].value = 0;


 info.inset = 0;
 info.states = 0; 
 Job_infos_init[id][0] = info;


 s = sys->get_initial_state(); //initial state
 if (partition_function(s)== id)
 {
   if (!Storages[id]->is_stored(s,ref))
      {
        Storages[id]->insert(s,ref);
        initial_ref = ref;
        Storages[id]->get_app_by_ref(ref,appendix);
        appendix.visited = info.inset;
        appendix.inset = info.inset;
        appendix.actions = 0;
        appendix.value = -1;
        appendix.is_seed = false;
        appendix.predecessors = new vector<thread_state_ref_t>;
        appendix.prob_predecessors = new vector<prob_thread_state_ref_t>; 
        appendix.actions_removed = new vector<prob_and_property_trans_t>;
        Storages[id]->set_app_by_ref(ref,appendix); 
        Queues_init[id][0]->push(ref);
      }
 }
 delete_state(s);
 //end of inicialization

 all_waiting = false;
 waiting_status[id].value = false; 
 inicialization[id].value = true;
 while(!all_waiting)
 {
   all_waiting = true;
   for(int i = 0; ( i < Thread_count && all_waiting); i++)
   {
     if ( !inicialization[i].value )
     {
       all_waiting = false;
     }
   }
 }

 if (id==0 && mode!='q') 
 {
    cout<<"AEC detection............................................................" << endl;
 }

 while(!Finnished)
 {
   waiting_signal = false;
   waiting_status[id].value = false; 

   while(system_locked)     // system is locked someone try to terminate the job --> wait
   {
     if(!waiting_signal)
     {
      // reading the buffer
       for(int i=0; i < Thread_count; i++)
     {
       if(i != id)
       { 
         buffer = &Buffers[id][i];
         while (!buffer->empty())
         {
           remote_state_t remote_state = buffer->front();
           buffer->pop();
           // process state
           queue_pointer = Queues_init[id][running_job]; //get queue iterator
           info = Job_infos_init[id][running_job]; //get info iterator
           if (running_job == 2 || running_job == 3 || running_job == 5) // backward reachibility
           {
             Storages[id]->get_app_by_ref(remote_state.ref,appendix);
             if (running_job == 2) //elimination
             {
               if(remote_state.probabilistic)
               {
                  if( info.inset == appendix.inset )
                  {
                    vector<prob_and_property_trans_t>::iterator iter_removed;
                    bool action_found = false;
                    for(iter_removed = appendix.actions_removed->begin();iter_removed != appendix.actions_removed->end() && !action_found;iter_removed++)
                    {
                      if(remote_state.action == (*iter_removed))
                      {
                        action_found = true;
                      }
                    }
                    if(!action_found)
                    {
                      appendix.actions--;
                      appendix.actions_removed->push_back(remote_state.action);
                    }
                    if(appendix.actions == 0)
                    {
                      queue_pointer->push(remote_state.ref);
                      appendix.inset = -1;
                    }
                    Storages[id]->set_app_by_ref(remote_state.ref,appendix);
                 }
               }
               else
               {
                 if( info.inset == appendix.inset )
                 {
                   appendix.actions--;
                   if(appendix.actions == 0)
                   {
                    queue_pointer->push(remote_state.ref);
                    appendix.inset = -1;
                   }
                   Storages[id]->set_app_by_ref(remote_state.ref,appendix);
                 }
               }
               if(mode == 'd') cout<<id<<" Sent state = "<<remote_state.ref<< " actions = "<<appendix.actions<<endl;
             }
             else //exploring
             {
                if( info.inset == appendix.inset)
                {
                  vector<prob_and_property_trans_t>::iterator iter_removed;
                  bool removed_action = false;
                  for(iter_removed = appendix.actions_removed->begin();iter_removed != appendix.actions_removed->end() && !removed_action && running_job == 3;iter_removed++)
                  {
                    if(remote_state.action == (*iter_removed))
                    {
                      removed_action = true;
                    }
                  }
                  if(! removed_action)
                  {
                    appendix.visited = info.inset;
                    appendix.inset = info.inset + 1;
                    queue_pointer->push(remote_state.ref);
                    if(running_job == 3)
                    {
                      Job_infos_init[id][running_job].states++;
                      //cout<<ref<<endl;
                    }
                    Storages[id]->set_app_by_ref(remote_state.ref,appendix);
                  }
                }
             }
           }
           else // foward reachibility
           {
             thread_state_ref.state_ref = remote_state.ref; 
             thread_state_ref.thread_id = i;
             if(running_job == 0) // the first reachibility
             {
               if ((int)(remote_state.action.prob_trans_gid) == -1)  // non-probabilistic transition
               {
                 if (!Storages[id]->is_stored(remote_state.state,ref)) 
                 {
                   Storages[id]->insert(remote_state.state,ref);
                   Storages[id]->get_app_by_ref(ref,appendix);
                   appendix.visited = info.inset;
                   appendix.inset = info.inset;
                   appendix.actions = 0;
                   appendix.value = -1;
                   appendix.is_seed = false;
                   appendix.predecessors = new vector<thread_state_ref_t>;
                   appendix.predecessors->push_back(thread_state_ref);
                   appendix.prob_predecessors = new vector<prob_thread_state_ref_t>; 
                   appendix.actions_removed = new vector<prob_and_property_trans_t>;
                   Storages[id]->set_app_by_ref(ref,appendix); 
                   queue_pointer->push(ref);
                 }
                 else
                 {
                   Storages[id]->get_app_by_ref(ref,appendix);
                   appendix.predecessors->push_back(thread_state_ref);
                   Storages[id]->set_app_by_ref(ref,appendix); 
                 }
               }
               else
               {
                 prob_thread_state_ref_t prob_thread_state_ref;
                 prob_thread_state_ref.thread_state_ref = thread_state_ref;
                 prob_thread_state_ref.action = remote_state.action;
                 if (!Storages[id]->is_stored(remote_state.state,ref))
                 {
                   Storages[id]->insert(remote_state.state,ref);
                   Storages[id]->get_app_by_ref(ref,appendix);
                   appendix.visited = info.inset;
                   appendix.inset = info.inset;
                   appendix.actions = 0;
                   appendix.value = -1;
                   appendix.is_seed = false;
                   appendix.predecessors = new vector<thread_state_ref_t>;
                   appendix.prob_predecessors = new vector<prob_thread_state_ref_t>;
                   appendix.prob_predecessors->push_back(prob_thread_state_ref); 
                   appendix.actions_removed = new vector<prob_and_property_trans_t>;
                   Storages[id]->set_app_by_ref(ref,appendix); 
                   queue_pointer->push(ref);
                 }
                 else
                 {
                   Storages[id]->get_app_by_ref(ref,appendix);
                   appendix.prob_predecessors->push_back(prob_thread_state_ref); 
                   Storages[id]->set_app_by_ref(ref,appendix); 
                 }
               }
             }
             else // some next reachibility
             {
               Storages[id]->is_stored(remote_state.state,ref);
               Storages[id]->get_app_by_ref(ref,appendix);
               if(running_job == 4)
               {
                 if(appendix.inset == info.inset) //seed
                 {
                   if(appendix.visited != info.inset) // not visited
                   {
                     appendix.visited = info.inset;
                     appendix.is_seed = true;
                     appendix.inset = info.inset + 2;
                     Storages[id]->set_app_by_ref(ref,appendix);
                     Queues_init[id][5]->push(ref);
                   }
                 }
                 else
                 {
                   if(appendix.visited != info.inset) // not visited
                   {
                     appendix.visited = info.inset;
                     appendix.inset = info.inset + 1;
                     Storages[id]->set_app_by_ref(ref,appendix);
                     queue_pointer->push(ref);
                   }
                 }
               }
               else
               {
                 if(appendix.inset == info.inset)
                 {
                    if(appendix.visited != info.inset)
                    {
                      appendix.visited = info.inset;
                      Storages[id]->set_app_by_ref(ref,appendix); 
                      queue_pointer->push(ref);
                    }
                 }
                 else
                 {
                   if(appendix.visited != info.inset)
                   {
                      appendix.visited = info.inset;
                      queue_pointer->push(ref);
                      Queues_init[id][2]->push(ref);
                      appendix.inset = -2;
                   }
                   Storages[id]->set_app_by_ref(ref,appendix); 
                 }
               }
             }
           }
           delete_state(remote_state.state);
         }
       }
     }
      // end of reading the buffer
      waiting_status[id].value = true;
      waiting_signal = true;
      //cout<<id<<"signal send"<<endl;
     }
   }
   waiting_status[id].value = false; 

   // do the job
   some_work = false;
   buffer_readed = false;
   n_cycles++ ;

   queue_pointer = Queues_init[id][running_job];
      if(! queue_pointer->empty())
      {
        info = Job_infos_init[id][running_job];  // get iterator corresponding info
        some_work = true;
        ref = queue_pointer->front();
        if(running_job == 0) // the first reachibility 
        {
          states[id]++;
        }
        if(mode == 'd') cout<<id<<" Job = "<<running_job<<" State = "<<ref<<endl;

        queue_pointer->pop();
        pred_ref = ref;
        // process state
        if(running_job == 2 || running_job == 3 || running_job == 5) // backward reachibility
        {
          Storages[id]->get_app_by_ref(pred_ref,pred_appendix);
          if(running_job == 2)
          {

            if(pred_appendix.inset != -2)
            {
              Job_infos_init[id][running_job].states++;
              if(mode == 'd') cout<<id<<" Counting state = "<<ref<<endl;
            }
          }
          if(running_job == 5)
          {
             Job_infos_init[id][running_job].states++;
          }
          for(pred_iter = pred_appendix.predecessors->begin(); pred_iter != pred_appendix.predecessors->end(); pred_iter++)
          {
            if(pred_iter->thread_id == id)
            {
              ref = pred_iter->state_ref;
              Storages[id]->get_app_by_ref(ref,appendix);
              if(running_job == 2) // elimination
              {
                if( info.inset == appendix.inset )
                {
                  if(mode == 'd') cout <<"     - "<<pred_ref<<" -> "<<ref<<endl;
                  appendix.actions--;
                  if(mode == 'd') cout <<ref<<" actions = "<<appendix.actions<<endl;
                  if(appendix.actions == 0)
                  {
                    queue_pointer->push(ref);
                    appendix.inset = -1;
                  }
                  Storages[id]->set_app_by_ref(ref,appendix);
                }
              }
              else //exploring
              {
                if(mode == 'd') cout <<"     - "<<pred_ref<<" -> "<<ref<<" appendix.inset = "<<appendix.inset<<" info.inset = "<<info.inset<<endl;
                if( info.inset == appendix.inset )
                {
                  appendix.visited = info.inset;
                  appendix.inset = info.inset + 1;
                  queue_pointer->push(ref);
                  if(running_job == 3)
                  {
                     Job_infos_init[id][running_job].states++;
                     //cout<<ref<<endl;
                  }
                  Storages[id]->set_app_by_ref(ref,appendix);
                }
              } 
            }
            else
            {
              // TO DO maybe should be better to check the Storages[partition_function(r)] 
              // if it is neccesery to send the state -- but not in the case of this storage implementation
              remote_state_t remote_state;
              remote_state.ref = pred_iter->state_ref;
              remote_state.probabilistic = false;
              Buffers[pred_iter->thread_id][id].push(remote_state);
            }
          }
          for(prob_pred_iter = pred_appendix.prob_predecessors->begin(); prob_pred_iter != pred_appendix.prob_predecessors->end(); prob_pred_iter++)
          {
            if(prob_pred_iter->thread_state_ref.thread_id == id)
            {
              ref = prob_pred_iter->thread_state_ref.state_ref;
              Storages[id]->get_app_by_ref(ref,appendix);
              if(running_job == 2) // elimination
              {
                if( info.inset == appendix.inset )
                {
                  if(mode == 'd') cout <<"     - "<<pred_ref<<" -> "<<ref<<endl;
                  vector<prob_and_property_trans_t>::iterator iter_removed;
                  bool action_found = false;
                  for(iter_removed = appendix.actions_removed->begin();iter_removed != appendix.actions_removed->end() && !action_found;iter_removed++)
                  {
                    if(prob_pred_iter->action == (*iter_removed))
                    {
                      action_found = true;
                    }
                  }
                  if(!action_found)
                  {
                    appendix.actions--;
                    if(mode == 'd') cout <<ref<<" actions = "<<appendix.actions<<endl;
                    appendix.actions_removed->push_back(prob_pred_iter->action);
                  }
                  if(appendix.actions == 0)
                  {
                    queue_pointer->push(ref);
                    appendix.inset = -1;
                  }
                  Storages[id]->set_app_by_ref(ref,appendix);
                }
              }
              else //exploring
              {
                if(mode == 'd') cout <<"     - "<<pred_ref<<" -> "<<ref<<" appendix.inset = "<<appendix.inset<<" info.inset = "<<info.inset<<endl;
                if( info.inset == appendix.inset)
                {
                  vector<prob_and_property_trans_t>::iterator iter_removed;
                  bool removed_action = false;
                  for(iter_removed = appendix.actions_removed->begin();iter_removed != appendix.actions_removed->end() && !removed_action && running_job == 3;iter_removed++)
                  {
                    if(prob_pred_iter->action == (*iter_removed))
                    {
                      removed_action = true;
                    }
                  }
                  if(! removed_action)
                  {
                    appendix.visited = info.inset;
                    appendix.inset = info.inset + 1;
                    queue_pointer->push(ref);
                    if(running_job == 3)
                    {
                      Job_infos_init[id][running_job].states++;
                      //cout<<ref<<endl;
                    }
                    Storages[id]->set_app_by_ref(ref,appendix);
                  }
                }
              } 
            }
            else
            {
              // TO DO maybe should be better to check the Storages[partition_function(r)] 
              // if it is neccesery to send the state -- but not in the case of this storage implementation
              remote_state_t remote_state;
              remote_state.ref = prob_pred_iter->thread_state_ref.state_ref;
              remote_state.probabilistic = true;
              remote_state.action =  prob_pred_iter->action;
              Buffers[prob_pred_iter->thread_state_ref.thread_id][id].push(remote_state);
            }
          }
        }
        else // foward reachibility 
        {
          if(running_job == 4)
          {
            Job_infos_init[id][running_job].states++;
            s = Storages[id]->reconstruct(ref);
            sys->get_succs_ordered_by_prob_and_property_trans(s,succs_cont);
            for (iter_i = succs_cont.begin(); iter_i != succs_cont.end(); iter_i++)
            {
              state_t r = iter_i->state;
              if (partition_function(r) == id)
              {
                 Storages[id]->is_stored(r,ref);
                 Storages[id]->get_app_by_ref(ref,appendix);
                 if(appendix.inset == info.inset) //seed
                 {
                   if(appendix.visited != info.inset) // not visited
                   {
                     appendix.visited = info.inset;
                     appendix.is_seed = true;
                     appendix.inset = info.inset + 2;
                     Storages[id]->set_app_by_ref(ref,appendix);
                     Queues_init[id][5]->push(ref);
                   }
                 }
                 else
                 {
                   if(appendix.visited != info.inset) // not visited
                   {
                     appendix.visited = info.inset;
                     appendix.inset = info.inset + 1;
                     Storages[id]->set_app_by_ref(ref,appendix);
                     queue_pointer->push(ref);
                   }
                 }
                 delete_state(r);
              }
              else
              {
                remote_state_t remote_state;
                remote_state.ref = pred_ref;
                remote_state.state = r;
                Buffers[partition_function(r)][id].push(remote_state);
              }
            }
            delete_state(s);
          }
          else
          {
            Storages[id]->get_app_by_ref(pred_ref,pred_appendix);
            pred_appendix.actions = 0;
            pred_appendix.actions_removed->clear();
            s = Storages[id]->reconstruct(ref);
            if (sys->is_accepting(s))
            {
              accepting_queues[id]->push(ref);
            }
            sys->get_succs_ordered_by_prob_and_property_trans(s,succs_cont);
            iter_i = succs_cont.begin();
            while(iter_i != succs_cont.end())
            {
              state_t r = iter_i->state;
              prob_and_property_trans_t previous_action;
              previous_action = (iter_i->prob_and_property_trans);
              if(running_job == 0) transitions[id]++;
              thread_state_ref.state_ref = pred_ref;
              thread_state_ref.thread_id = id;

              if ((int)(previous_action.prob_trans_gid) == -1)  // non-probabilistic transition
              {
                pred_appendix.actions++;
                if (partition_function(r) == id)
                {
                  if(running_job == 0)  // the first forward reachibility
                  {

                    if (!Storages[id]->is_stored(r,ref))
                    {
                      Storages[id]->insert(r,ref);
                      Storages[id]->get_app_by_ref(ref,appendix);
                      appendix.visited = info.inset;
                      appendix.inset = info.inset;
                      appendix.actions = 0;
                      appendix.value = -1;
                      appendix.is_seed = false;
                      appendix.predecessors = new vector<thread_state_ref_t>;
                      appendix.predecessors->push_back(thread_state_ref);
                      appendix.prob_predecessors = new vector<prob_thread_state_ref_t>; 
                      appendix.actions_removed = new vector<prob_and_property_trans_t>;
                      Storages[id]->set_app_by_ref(ref,appendix); 
                      queue_pointer->push(ref);
                    }
                    else
                    {
                      Storages[id]->get_app_by_ref(ref,appendix);
                      appendix.predecessors->push_back(thread_state_ref);
                      Storages[id]->set_app_by_ref(ref,appendix); 
                    }
                  }
                  else
                  {
                     Storages[id]->is_stored(r,ref);
                     Storages[id]->get_app_by_ref(ref,appendix);
                     if(appendix.inset == info.inset)
                     {
                       if(appendix.visited != info.inset)
                       {
                         appendix.visited = info.inset;
                         Storages[id]->set_app_by_ref(ref,appendix);
                         queue_pointer->push(ref);
                       }
                     }
                     else
                     {
                        if(appendix.visited != info.inset)
                        {
                          appendix.visited = info.inset;
                          queue_pointer->push(ref);
                          Queues_init[id][2]->push(ref);
                          appendix.inset = -2;
                          Storages[id]->set_app_by_ref(ref,appendix);
                        }
                     }
                  }
                  delete_state(r);
                  if(mode == 'd') cout <<"    neprob - "<<pred_ref<<" -> "<<ref<<endl;
                }
                else
                {
                  remote_state_t remote_state;
                  remote_state.ref = pred_ref;
                  remote_state.action = previous_action;
                  remote_state.state = r;
                  Buffers[partition_function(r)][id].push(remote_state);
                }
                iter_i++;
              }
              else  // probabilistic transition
              {
                 prob_thread_state_ref_t prob_thread_state_ref;
                 prob_thread_state_ref.thread_state_ref = thread_state_ref;
                 prob_thread_state_ref.action = previous_action;
                 pred_appendix.actions++;
                 if (partition_function(r) == id)
                 {
                   if(running_job == 0)  // the first forward reachibility
                   {
                     if (!Storages[id]->is_stored(r,ref))
                     {
                       Storages[id]->insert(r,ref);
                       Storages[id]->get_app_by_ref(ref,appendix);
                       appendix.visited = info.inset;
                       appendix.inset = info.inset;
                       appendix.actions = 0;
                       appendix.value = -1;
                       appendix.is_seed = false;
                       appendix.predecessors = new vector<thread_state_ref_t>;
                       appendix.prob_predecessors = new vector<prob_thread_state_ref_t>;
                       appendix.prob_predecessors->push_back(prob_thread_state_ref); 
                       appendix.actions_removed = new vector<prob_and_property_trans_t>;
                       Storages[id]->set_app_by_ref(ref,appendix); 
                       queue_pointer->push(ref);
                     }
                     else
                     {
                       Storages[id]->get_app_by_ref(ref,appendix);
                       appendix.prob_predecessors->push_back(prob_thread_state_ref); 
                       Storages[id]->set_app_by_ref(ref,appendix); 
                     }
                   }
                   else
                   {
                     Storages[id]->is_stored(r,ref);
                     Storages[id]->get_app_by_ref(ref,appendix);
                     if(appendix.inset == info.inset)
                     {
                        if(appendix.visited != info.inset)
                        {
                          appendix.visited = info.inset;
                          Storages[id]->set_app_by_ref(ref,appendix);
                          queue_pointer->push(ref);
                        }
                     }
                     else
                     {
                        if(appendix.visited != info.inset)
                        {
                          appendix.visited = info.inset;
                          Queues_init[id][2]->push(ref);
                          appendix.inset = -2;
                          Storages[id]->set_app_by_ref(ref,appendix);
                          queue_pointer->push(ref);
                        }
                     }

                   }
                   delete_state(r);
                   if(mode == 'd') cout <<"   prob 1 - "<<pred_ref<<" -> "<<ref<<endl;
                 }
                 else
                 {
                   remote_state_t remote_state;
                   remote_state.ref = pred_ref;
                   remote_state.action = previous_action;
                   remote_state.state = r;
                   Buffers[partition_function(r)][id].push(remote_state);
                 }
                 iter_i++;
                 while (iter_i!=succs_cont.end() && previous_action == ((*iter_i).prob_and_property_trans))
                 {
                   state_t rr = iter_i->state;
                   transitions[id]++;
                   prob_thread_state_ref.action = previous_action;
                   if (partition_function(rr) == id)
                   {
                     if(running_job == 0)  // the first forward reachibility
                     {
                       if (!Storages[id]->is_stored(rr,ref))
                       {
                         Storages[id]->insert(rr,ref);
                         Storages[id]->get_app_by_ref(ref,appendix);
                         appendix.visited = info.inset;
                         appendix.inset = info.inset;
                         appendix.actions = 0;
                         appendix.is_seed = false;
                         appendix.value = -1;
                         appendix.predecessors = new vector<thread_state_ref_t>;
                         appendix.prob_predecessors = new vector<prob_thread_state_ref_t>;
                         appendix.prob_predecessors->push_back(prob_thread_state_ref); 
                         appendix.actions_removed = new vector<prob_and_property_trans_t>;
                         Storages[id]->set_app_by_ref(ref,appendix); 
                         queue_pointer->push(ref);
                       }
                       else
                       {
                         Storages[id]->get_app_by_ref(ref,appendix);
                         appendix.prob_predecessors->push_back(prob_thread_state_ref); 
                         Storages[id]->set_app_by_ref(ref,appendix); 
                       }
                    }
                    else
                    {
                       Storages[id]->is_stored(rr,ref);
                       Storages[id]->get_app_by_ref(ref,appendix);
                       if(appendix.inset == info.inset)
                       {
                         if(appendix.visited != info.inset)
                         {
                            appendix.visited = info.inset;
                            Storages[id]->set_app_by_ref(ref,appendix);
                            queue_pointer->push(ref);
                         }
                       }
                       else
                       {
                         if(appendix.visited != info.inset)
                         {
                            appendix.visited = info.inset;
                            Queues_init[id][2]->push(ref);
                            appendix.inset = -2;
                            Storages[id]->set_app_by_ref(ref,appendix);
                            queue_pointer->push(ref);
                         }
                      }
                    }
                    if(mode == 'd') cout <<"    prob 2- "<<pred_ref<<" -> "<<ref<<endl;
                    delete_state(rr);
                  }
                  else
                  {
                     remote_state_t remote_state;
                     remote_state.ref = pred_ref;
                     remote_state.action = previous_action;
                     remote_state.state = rr;
                     Buffers[partition_function(rr)][id].push(remote_state);
                  }
                  iter_i++;
                }
              }
            }
            if(mode == 'd') cout <<pred_ref<<" actions = "<<pred_appendix.actions<<endl;
            if(pred_appendix.inset == info.inset)
            {
              Job_infos_init[id][running_job].states++;
            }
            Storages[id]->set_app_by_ref(pred_ref,pred_appendix);
            delete_state(s);
          }
        }
     }
   if( (n_cycles == WHEN_READ_FROM_BUFFER || !some_work) && !Finnished) //begin of reading from buffer
   {
     if(n_cycles == WHEN_READ_FROM_BUFFER) n_cycles = 0;

     buffer_readed = true;

     for(int i=0; i < Thread_count; i++)
     {
       if(i != id)
       { 
         buffer = &Buffers[id][i];
         while (!buffer->empty())
         {
           remote_state_t remote_state = buffer->front();
           buffer->pop();
           // process state
           queue_pointer = Queues_init[id][running_job]; //get queue iterator
           info = Job_infos_init[id][running_job]; //get info iterator
           if (running_job == 2 || running_job == 3 || running_job == 5) // backward reachibility
           {
             Storages[id]->get_app_by_ref(remote_state.ref,appendix);
             if (running_job == 2) //elimination
             {
               if(remote_state.probabilistic)
               {
                  if( info.inset == appendix.inset )
                  {
                    vector<prob_and_property_trans_t>::iterator iter_removed;
                    bool action_found = false;
                    for(iter_removed = appendix.actions_removed->begin();iter_removed != appendix.actions_removed->end() && !action_found;iter_removed++)
                    {
                      if(remote_state.action == (*iter_removed))
                      {
                        action_found = true;
                      }
                    }
                    if(!action_found)
                    {
                      appendix.actions--;
                      appendix.actions_removed->push_back(remote_state.action);
                    }
                    if(appendix.actions == 0)
                    {
                      queue_pointer->push(remote_state.ref);
                      appendix.inset = -1;
                    }
                    Storages[id]->set_app_by_ref(remote_state.ref,appendix);
                 }
               }
               else
               {
                 if( info.inset == appendix.inset )
                 {
                   appendix.actions--;
                   if(appendix.actions == 0)
                   {
                    queue_pointer->push(remote_state.ref);
                    appendix.inset = -1;
                   }
                   Storages[id]->set_app_by_ref(remote_state.ref,appendix);
                 }
               }
               if(mode == 'd') cout<<id<<" Sent state = "<<remote_state.ref<< " actions = "<<appendix.actions<<endl;
             }
             else //exploring
             {
                if( info.inset == appendix.inset)
                {
                  vector<prob_and_property_trans_t>::iterator iter_removed;
                  bool removed_action = false;
                  for(iter_removed = appendix.actions_removed->begin();iter_removed != appendix.actions_removed->end() && !removed_action && running_job == 3;iter_removed++)
                  {
                    if(remote_state.action == (*iter_removed))
                    {
                      removed_action = true;
                    }
                  }
                  if(! removed_action)
                  {
                    appendix.visited = info.inset;
                    appendix.inset = info.inset + 1;
                    queue_pointer->push(remote_state.ref);
                    if(running_job == 3)
                    {
                      Job_infos_init[id][running_job].states++;
                      //cout<<ref<<endl;
                    }
                    Storages[id]->set_app_by_ref(remote_state.ref,appendix);
                  }
                }
             }
           }
           else // foward reachibility
           {
             thread_state_ref.state_ref = remote_state.ref; 
             thread_state_ref.thread_id = i;
             if(running_job == 0) // the first reachibility
             {
               if ((int)(remote_state.action.prob_trans_gid) == -1)  // non-probabilistic transition
               {
                 if (!Storages[id]->is_stored(remote_state.state,ref)) 
                 {
                   Storages[id]->insert(remote_state.state,ref);
                   Storages[id]->get_app_by_ref(ref,appendix);
                   appendix.visited = info.inset;
                   appendix.inset = info.inset;
                   appendix.actions = 0;
                   appendix.value = -1;
                   appendix.is_seed = false;
                   appendix.predecessors = new vector<thread_state_ref_t>;
                   appendix.predecessors->push_back(thread_state_ref);
                   appendix.prob_predecessors = new vector<prob_thread_state_ref_t>; 
                   appendix.actions_removed = new vector<prob_and_property_trans_t>;
                   Storages[id]->set_app_by_ref(ref,appendix); 
                   queue_pointer->push(ref);
                 }
                 else
                 {
                   Storages[id]->get_app_by_ref(ref,appendix);
                   appendix.predecessors->push_back(thread_state_ref);
                   Storages[id]->set_app_by_ref(ref,appendix); 
                 }
               }
               else
               {
                 prob_thread_state_ref_t prob_thread_state_ref;
                 prob_thread_state_ref.thread_state_ref = thread_state_ref;
                 prob_thread_state_ref.action = remote_state.action;
                 if (!Storages[id]->is_stored(remote_state.state,ref))
                 {
                   Storages[id]->insert(remote_state.state,ref);
                   Storages[id]->get_app_by_ref(ref,appendix);
                   appendix.visited = info.inset;
                   appendix.inset = info.inset;
                   appendix.actions = 0;
                   appendix.value = -1;
                   appendix.is_seed = false;
                   appendix.predecessors = new vector<thread_state_ref_t>;
                   appendix.prob_predecessors = new vector<prob_thread_state_ref_t>;
                   appendix.prob_predecessors->push_back(prob_thread_state_ref); 
                   appendix.actions_removed = new vector<prob_and_property_trans_t>;
                   Storages[id]->set_app_by_ref(ref,appendix); 
                   queue_pointer->push(ref);
                 }
                 else
                 {
                   Storages[id]->get_app_by_ref(ref,appendix);
                   appendix.prob_predecessors->push_back(prob_thread_state_ref); 
                   Storages[id]->set_app_by_ref(ref,appendix); 
                 }
               }
             }
             else // some next reachibility
             {
               Storages[id]->is_stored(remote_state.state,ref);
               Storages[id]->get_app_by_ref(ref,appendix);
               if(running_job == 4)
               {
                 if(appendix.inset == info.inset) //seed
                 {
                   if(appendix.visited != info.inset) // not visited
                   {
                     appendix.visited = info.inset;
                     appendix.is_seed = true;
                     appendix.inset = info.inset + 2;
                     Storages[id]->set_app_by_ref(ref,appendix);
                     Queues_init[id][5]->push(ref);
                   }
                 }
                 else
                 {
                   if(appendix.visited != info.inset) // not visited
                   {
                     appendix.visited = info.inset;
                     appendix.inset = info.inset + 1;
                     Storages[id]->set_app_by_ref(ref,appendix);
                     queue_pointer->push(ref);
                   }
                 }
               }
               else
               {
                 if(appendix.inset == info.inset)
                 {
                    if(appendix.visited != info.inset)
                    {
                      appendix.visited = info.inset;
                      Storages[id]->set_app_by_ref(ref,appendix); 
                      queue_pointer->push(ref);
                    }
                 }
                 else
                 {
                   if(appendix.visited != info.inset)
                   {
                      appendix.visited = info.inset;
                      queue_pointer->push(ref);
                      Queues_init[id][2]->push(ref);
                      appendix.inset = -2;
                   }
                   Storages[id]->set_app_by_ref(ref,appendix); 
                 }
               }
             }
           }
           delete_state(remote_state.state);
         }
       }
     }
   } //end of reading from buffer

   // termination detection
    bool terminating = false;
    if (buffer_readed)
    {
       if(Queues_init[id][running_job]->empty())
       {
         terminating = true;
       }
    }


    if (terminating)
    { 
      for(int i=0; (i < Thread_count && terminating); i++)  // check the corresponding queues system is unlocked
      {
        if (!Queues_init[i][running_job]->empty() ) terminating = false;  // the queue is non-empty
      }
    }

        if (terminating && !Finnished) // try lock evrything and try to terminate the job 
        {
           if (pthread_mutex_trylock(&mutex) == 0 )
           {
              //cout<<id<<" detecting the termination"<<endl;
              system_locked = true; // system locked - wait until others threads will wait then try to terminate the job 

              waiting_signal = false;
              while(! waiting_signal)
              {
                all_waiting = true;
                for(int i = 0; ( i < Thread_count && all_waiting); i++)
                 if ( (i != id) && (!waiting_status[i].value) ) all_waiting = false;

                if(all_waiting) waiting_signal = true; //all others threads are waiting
              }

              for(int i=0; (i < Thread_count && terminating); i++) 
              {
                if (! Queues_init[i][running_job]->empty() ) terminating = false  ; // check the corresponding queues system is locked
              } 
              if (terminating) //the correponding queues are empty check all buffers
              {
                for(int i=0; (i < Thread_count && terminating); i++)
                 for(int j=0; (j < Thread_count && terminating); j++)
                   if(i != j && terminating)
                   { 
                     buffer = &Buffers[i][j];  //from j to i
                     while (!buffer->empty() && terminating)
                     {
                      remote_state_t remote_state = buffer->front();
                      buffer->pop();
                      // process state
                      queue_pointer = Queues_init[i][running_job]; //get queue iterator
                      info = Job_infos_init[i][running_job]; //get info iterator
                      if (running_job == 2 || running_job == 3 || running_job == 5) // backward reachibility
                      {
                        Storages[i]->get_app_by_ref(remote_state.ref,appendix);
                        if (running_job == 2) //elimination
                        {
                          if(remote_state.probabilistic)
                          {
                            if( info.inset == appendix.inset )
                            {
                              vector<prob_and_property_trans_t>::iterator iter_removed;
                              bool action_found = false;
                              for(iter_removed = appendix.actions_removed->begin();iter_removed != appendix.actions_removed->end() && !action_found;iter_removed++)
                              {
                                if(remote_state.action == (*iter_removed))
                                {
                                  action_found = true;
                                }
                              }
                              if(!action_found)
                              {
                                appendix.actions--;
                                appendix.actions_removed->push_back(remote_state.action);
                              }
                              if(appendix.actions == 0)
                              {
                                queue_pointer->push(remote_state.ref);
                                appendix.inset = -1;
                                terminating = false;
                              }
                              Storages[i]->set_app_by_ref(remote_state.ref,appendix);
                            }
                          }
                          else
                          {
                            if( info.inset == appendix.inset )
                            {
                              appendix.actions--;
                              if(appendix.actions == 0)
                              {
                                queue_pointer->push(remote_state.ref);
                                appendix.inset = -1;
                                terminating = false;
                              }
                              Storages[i]->set_app_by_ref(remote_state.ref,appendix);
                            }
                          }
                          if(mode == 'd') cout<<id<<" Sended state = "<<remote_state.ref<< " actions = "<<appendix.actions<<endl;
                        }
                        else //exploring
                        {
                          if( info.inset == appendix.inset)
                          {
                             vector<prob_and_property_trans_t>::iterator iter_removed;
                             bool removed_action = false;
                             for(iter_removed = appendix.actions_removed->begin();iter_removed != appendix.actions_removed->end() && !removed_action && running_job == 3;iter_removed++)
                             {
                               if(remote_state.action == (*iter_removed))
                               {
                                 removed_action = true;
                               }
                             }
                             if(! removed_action)
                             {
                               appendix.visited = info.inset;
                               appendix.inset = info.inset + 1;
                               queue_pointer->push(remote_state.ref);
                               terminating = false;
                               if(running_job == 3)
                               {
                                 Job_infos_init[i][running_job].states++;
                                 //cout<<ref<<endl;
                               }
                               Storages[i]->set_app_by_ref(remote_state.ref,appendix);
                             }
                           }
                        }
                      }
                      else // foward reachibility
                      {
                        thread_state_ref.state_ref = remote_state.ref; 
                        thread_state_ref.thread_id = j;
                        if(running_job == 0) // the first reachibility
                        {
                          if ((int)(remote_state.action.prob_trans_gid) == -1)  // non-probabilistic transition
                          {
                            if (!Storages[i]->is_stored(remote_state.state,ref)) 
                            {
                              Storages[i]->insert(remote_state.state,ref);
                              Storages[i]->get_app_by_ref(ref,appendix);
                              appendix.visited = info.inset;
                              appendix.inset = info.inset;
                              appendix.actions = 0;
                              appendix.value = -1;
                              appendix.is_seed = false;
                              appendix.predecessors = new vector<thread_state_ref_t>;
                              appendix.predecessors->push_back(thread_state_ref);
                              appendix.prob_predecessors = new vector<prob_thread_state_ref_t>; 
                              appendix.actions_removed = new vector<prob_and_property_trans_t>;
                              Storages[i]->set_app_by_ref(ref,appendix); 
                              queue_pointer->push(ref);
                              terminating = false;
                            }
                            else
                            {
                              Storages[i]->get_app_by_ref(ref,appendix);
                              appendix.predecessors->push_back(thread_state_ref);
                              Storages[i]->set_app_by_ref(ref,appendix); 
                            }
                          }
                          else
                          {
                            prob_thread_state_ref_t prob_thread_state_ref;
                            prob_thread_state_ref.thread_state_ref = thread_state_ref;
                            prob_thread_state_ref.action = remote_state.action;
                            if (!Storages[i]->is_stored(remote_state.state,ref))
                            {
                              Storages[i]->insert(remote_state.state,ref);
                              Storages[i]->get_app_by_ref(ref,appendix);
                              appendix.visited = info.inset;
                              appendix.inset = info.inset;
                              appendix.actions = 0;
                              appendix.value = -1;
                              appendix.is_seed = false;
                              appendix.predecessors = new vector<thread_state_ref_t>;
                              appendix.prob_predecessors = new vector<prob_thread_state_ref_t>;
                              appendix.prob_predecessors->push_back(prob_thread_state_ref); 
                              appendix.actions_removed = new vector<prob_and_property_trans_t>;
                              Storages[i]->set_app_by_ref(ref,appendix); 
                              queue_pointer->push(ref);  
                              terminating = false;
                            }
                            else
                            {
                              Storages[i]->get_app_by_ref(ref,appendix);
                              appendix.prob_predecessors->push_back(prob_thread_state_ref); 
                              Storages[i]->set_app_by_ref(ref,appendix); 
                            }
                          }
                        }
                        else // some next reachibility
                        {
                          Storages[i]->is_stored(remote_state.state,ref);
                          Storages[i]->get_app_by_ref(ref,appendix);
                          if(running_job == 4)
                          {
                            if(appendix.inset == info.inset) //seed
                            {
                              if(appendix.visited != info.inset) // not visited
                              {
                                appendix.visited = info.inset;
                                appendix.is_seed = true;
                                appendix.inset = info.inset + 2;
                                Storages[i]->set_app_by_ref(ref,appendix);
                                Queues_init[i][5]->push(ref);
                                terminating = false;
                              }
                            }
                            else
                            {
                              if(appendix.visited != info.inset) // not visited
                              {
                                 appendix.visited = info.inset;
                                 appendix.inset = info.inset + 1;
                                 Storages[i]->set_app_by_ref(ref,appendix);
                                 queue_pointer->push(ref);
                                 terminating = false;
                              }
                            }
                          }
                          else
                          {
                            if(appendix.inset == info.inset)
                            {
                               if(appendix.visited != info.inset)
                               {
                                  appendix.visited = info.inset;
                                  Storages[i]->set_app_by_ref(ref,appendix); 
                                  queue_pointer->push(ref);
                                  terminating = false;
                               }
                            }
                            else
                            {
                              if(appendix.visited != info.inset)
                              {
                                appendix.visited = info.inset;
                                queue_pointer->push(ref);
                                Queues_init[i][2]->push(ref);
                                appendix.inset = -2;
                                terminating = false;
                              }
                              Storages[i]->set_app_by_ref(ref,appendix); 
                            }
                          }
                        }
                      }
                      delete_state(remote_state.state); 
                    }
                  }
              }
              if (terminating) //the corresponding queues and all buffers are empty --> terminate a job
              {
                if(mode == 'd') cout<<id<<" Starting new job(). Terminating job = "<<running_job <<endl;
                if(running_job == 0) //terminated job is the first reachibility
                {
                   // State space info 
                   for(int i=0; i < Thread_count; i++)
                   {
                     States = States + states[i];
                     Transitions = Transitions + transitions[i];
                   }

                   for(int i=0;i < Thread_count; i++)
                   {
                     //create new queue and info
                     queue_pointer = new queue<state_ref_t>(*accepting_queues[i]);
                     if(mode == 'd') cout<<id<<" Size of accepting queue = "<<accepting_queues[i]->size()<<endl;

                      //create the new info
                       info.inset = Job_infos_init[i][0].inset;
                       info.states = 0;
                       Job_infos_init[i][2] = info;

                       running_job = 2;
                   }
                   if(mode == 'd') cout<<id<<" Job 2 started"<<endl;
                }
                else // terminated job is not the first reachibility
                {
                  switch(running_job)
                  {
                    case 1:    // terminated job is forward reachibility
                      for(int i=0;i < Thread_count; i++)
                      {

                         //create the new info
                         if(mode == 'd') cout<<id<<" Size of accepting queue = "<<accepting_queues[i]->size()<<endl;
                         if(mode == 'd') cout<<id<<" Size of eliminating queue = "<<Queues_init[i][2]->size()<<endl;
                         info.inset = Job_infos_init[i][1].inset;
                         info.states = 0;
                         Job_infos_init[i][2] = info;

                         running_job = 2;
                      }
                      if(mode == 'd') cout<<id<<" Job 2 started"<<endl;
                      break;
                    case 2:   // terminated job is elimination
                      for(int i=0;i < Thread_count; i++)
                      {
                        queue_pointer = new queue<state_ref_t>;
                        while(!accepting_queues[i]->empty())
                        {
                           ref = accepting_queues[i]->front();
                           accepting_queues[i]->pop();
                           Storages[i]->get_app_by_ref(ref,appendix);
                           if(appendix.inset == Job_infos_init[i][2].inset)
                           {
                             queue_pointer->push(ref);
                             if(mode == 'd') cout<<id<<" Accepting state = "<<ref<<endl;
                           }
                        } 
                        info.inset = Job_infos_init[i][2].inset;
                        info.states = 0;
                        Queues_init[i][3] =  queue_pointer;
                        //Queues_init[i][1] =  new queue<state_ref_t>(*(queue_pointer)); // starting states for next forward reachbility
                        Job_infos_init[i][3] = info;

                        running_job = 3;
                      }
                      if(mode == 'd') cout<<id<<" Job 3 started"<<endl;
                      break;
                    case 3:  // terminated job is exploring
                      previous_explored_state = tmp_explored_state;
                      tmp_reached_state = 0;
                      tmp_explored_state = 0;
                      tmp_eliminated_state = 0;
                      for(int i=0;i < Thread_count; i++)
                      {
                         if(Job_infos_init[id][3].inset == 0)
                         {
                           tmp_reached_state = States;
                         }
                         else
                         {
                           tmp_reached_state = tmp_reached_state + Job_infos_init[i][1].states;
                         } 
                         tmp_explored_state = tmp_explored_state + Job_infos_init[i][3].states;
                         tmp_eliminated_state = tmp_eliminated_state + Job_infos_init[i][2].states;
                      }
                      if(mode != 'q')
                      {
                        cout<<"iteration:                   "<<Job_infos_init[id][3].inset + 1<<endl;
                        cout<<"reached states:              "<<tmp_reached_state<<endl;
                        cout<<"explored states:             "<<tmp_explored_state<<endl;
                        cout<<"eliminated states:           "<<tmp_eliminated_state<<endl;
                        cout<<"time:                        "<<timer.gettime()<<" s"<<endl;
                        cout <<"-------------------------------------------------------------------------"<<endl;
                      }
                      if(previous_explored_state == tmp_explored_state || tmp_explored_state == 0 || tmp_reached_state == tmp_explored_state + tmp_eliminated_state )
                      {
                        if (tmp_explored_state != 0)
                        {
                            if (mode!='q') cout <<"AEC detection.......................................................Done."<<endl;
                            cout <<"========================================================================="<<endl;
                            cout <<"An accepting end component found."<<endl
                                 <<"LTL formula is not satisfied with probability 1."<<endl;
                            cout <<"========================================================================="<<endl;

                            if(type == 'l') Finnished = true;

                            s = sys->get_initial_state(); //initial state
                            Storages[partition_function(s)]->is_stored(s,ref);
                            Storages[partition_function(s)]->get_app_by_ref(ref,appendix);
                            if(appendix.inset == Job_infos_init[id][3].inset + 1 && !Finnished)
                            {
                              Finnished = true;
                              cout <<"======================================================================="<<endl;
                              cout <<"LTL formula is satisfied with probability 0."<<endl;
                              cout <<"======================================================================="<<endl;
                            } 
                            else
                            {
                              if(mode == 'd') cout<<id<<" Job 4 started"<<endl;
                              if (mode!='q')
                              {
                                cout<<"Seeds searching.....................................................";
                                cout.flush();
                              }
                              for(int i=0;i < Thread_count; i++)
                              {
                                 info.inset = Job_infos_init[i][3].inset + 1;
                                 info.states = 0;
                                 Job_infos_init[i][4] = info;
                                 if (partition_function(s)== i)
                                 {
                                   appendix.visited = info.inset;
                                   appendix.inset = info.inset + 1;
                                   Storages[i]->set_app_by_ref(ref,appendix);
                                   Queues_init[i][4]->push(ref);
                                 }
                               }
                               running_job = 4;
                            }
                        }
                        else
                        {  if (mode!='q') cout <<"AEC detection.......................................................Done."<<endl;
                           cout <<"=========================================================================="<<endl;
                           cout <<"No accepting end component found. "<<endl
                                <<"LTL formula satisfied with probability 1."<<endl;
                           cout <<"=========================================================================="<<endl;
                           Finnished = true;
                        }
                      }
                      else
                      {
                        if(mode == 'd') cout<<id<<" Job 1 started"<<endl;
                        s = sys->get_initial_state(); //initial state
                        for(int i=0;i < Thread_count; i++)
                        {
                          info.inset = Job_infos_init[i][3].inset + 1;
                          info.states = 0;
                          Job_infos_init[i][1] = info;
                          if (partition_function(s)== i)
                          {
                            Storages[i]->is_stored(s,ref);
                            Storages[i]->get_app_by_ref(ref,appendix);
                            appendix.visited = info.inset;
                            Storages[i]->set_app_by_ref(ref,appendix);
                            Queues_init[i][1]->push(ref);
                          }
                        }

                        running_job = 1;
                      }
                      break;
                    case 4: // terminated job is seed detecting
                     if(mode == 'd') cout<<id<<" Job 5 started"<<endl;
                     tmp_reached_state = 0;
                     seeds = 0;
                     for(int i=0;i < Thread_count; i++)
                     {
                       tmp_reached_state = tmp_reached_state + Job_infos_init[i][4].states;
                       seeds = seeds + Queues_init[i][5]->size();
                       info.inset = Job_infos_init[i][4].inset + 1;
                       info.states = 0;
                       Job_infos_init[i][5] = info;
                     }
                     running_job = 5;
                     if(mode != 'q')
                     { 
                       cout<<"Done."<<endl;
                       cout <<"-------------------------------------------------------------------------"<<endl;
                       cout<<"reached states:              "<<tmp_reached_state<<endl;
                       cout<<"seeds:                       "<<seeds<<endl;
                       cout<<"time:                        "<<timer.gettime()<<" s"<<endl;
                       cout <<"-------------------------------------------------------------------------"<<endl;
                       cout <<"Minimal subgraph identification.....................................";
                       cout.flush();
                     }
                     break;
                   case 5: // terminated job is seed detecting
                     tmp_explored_state = 0;
                     Working_set = Job_infos_init[id][5].inset +1;
                     for(int i=0;i < Thread_count; i++)
                     {
                       tmp_explored_state = tmp_explored_state + Job_infos_init[i][5].states;
                     }
                     if(mode != 'q')
                     {
                       cout<<"Done."<<endl;
                       cout <<"-------------------------------------------------------------------------"<<endl;
                       cout<<"states in minimal subgraph:  "<<tmp_explored_state<<endl;
                       cout<<"time:                        "<<timer.gettime()<<" s"<<endl;
                       cout <<"-------------------------------------------------------------------------"<<endl;
                       cout <<"SCC decomposition...................................................";
                       cout.flush();
                     }
                     Finnished = true;
                     break;
                  } //end of switch
                }
              }
              system_locked = false; //unlock the system 
              waiting_signal = false;
              while(! waiting_signal)
              {
                all_waiting = true;
                for(int i = 0; ( i < Thread_count && all_waiting); i++)
                 if ( (i != id) && (waiting_status[i].value) ) all_waiting = false;

                if(all_waiting) waiting_signal = true; //all others threads are not waiting
              }
              pthread_mutex_unlock(&mutex); // free the mutex
           }
         }

 
     //end of termination detection
 } //all job terminated
//Alfaro + graph reduction finished


all_waiting = false;
synchronization_alfaro[id].value = true;
while(!all_waiting)
{
   all_waiting = true;
   for(int i = 0; ( i < Thread_count && all_waiting); i++)
   {
     if ( !synchronization_alfaro[i].value )
     {
       all_waiting = false;
     }
   }
}

if(id == 0)
{
scc_timer.reset();
}

if(Working_set != -1)
{


  map<int,queue<state_ref_t>*>::iterator queues_iter,term_queues_iter;
  map<int,info_t>::iterator info_iter;

  last_job = Working_set + 1;

 //inicialization of new job - the first reachibility 
 queue_pointer = new queue<state_ref_t>();
 Queues[id].insert(make_pair(last_job,queue_pointer));
 
 info.tag = 0;
 info.inset = Working_set;
 info.states = 0; 
 info.reached = new queue<state_ref_t>(); 
 info.range = 0;
 info.executed_by = 0;
 Job_infos[id].insert(make_pair(last_job,info));

 s = sys->get_initial_state(); //initial state
 if (partition_function(s)== id)
 {
   Storages[id]->is_stored(s,ref);
   Storages[id]->get_app_by_ref(ref,appendix);
   if(appendix.inset == info.inset && appendix.visited != last_job)
   {
     appendix.visited = last_job;
     appendix.predecessors->clear();
     Storages[id]->set_app_by_ref(ref,appendix); 
     queue_pointer->push(ref);
   }
 }
 delete_state(s);
 //end of inicialization

 while(Queues[id].size() != 0) // if Queues[id].size() == 0 end of the algorithm
 {

   if (max_size < Queues[id].size() ) max_size = Queues[id].size();

   waiting_signal = false;
   waiting_status[id].value = false; 
   creating_status[id].value = 0;

   while(system_locked)     // system is locked someone try to terminate the job --> wait
   {
     if(!waiting_signal)
     {
      // reading the buffer
      for(int i=0; i < Thread_count; i++)
      {
        if(i != id)
        { 
          buffer = &Buffers[id][i];
          while (!buffer->empty())
          {
            remote_state_t remote_state = buffer->front();
            buffer->pop();
            // process state
            queues_iter = Queues[id].find(remote_state.job); //get queue iterator
            info_iter = Job_infos[id].find(remote_state.job); //get info iterator
            if (remote_state.tag == 1) // backward reachibility
            {
              Storages[id]->get_app_by_ref(remote_state.ref,appendix);
              if( info_iter->second.inset == appendix.inset && appendix.visited != queues_iter->first)
              {
                appendix.visited = queues_iter->first;
                appendix.inset = queues_iter->first;
                queues_iter->second->push(remote_state.ref);
                Storages[id]->set_app_by_ref(remote_state.ref,appendix);
              } 
            }
            else // foward reachibility or owcty
            {
              thread_state_ref.state_ref = remote_state.ref; 
              thread_state_ref.thread_id = i;
              if(queues_iter->first == Working_set + 1) // the first reachibility
              {
                 Storages[id]->is_stored(remote_state.state,ref);
                 Storages[id]->get_app_by_ref(ref,appendix);
                 if (appendix.inset == info_iter->second.inset)  
                 {
                   if(appendix.visited != queues_iter->first)
                   {
                     appendix.visited = queues_iter->first;
                     appendix.predecessors->clear();
                     appendix.predecessors->push_back(thread_state_ref);
                     Storages[id]->set_app_by_ref(ref,appendix); 
                     queues_iter->second->push(ref);
                   }
                   else
                   {
                     appendix.predecessors->push_back(thread_state_ref);
                     Storages[id]->set_app_by_ref(ref,appendix); 
                   }
                 }
              }
              else
              {
                Storages[id]->is_stored(remote_state.state,ref);
                Storages[id]->get_app_by_ref(ref,appendix);
                if (remote_state.tag == 2) // owcty
                {
                  if(appendix.inset == info_iter->second.inset)
                  {
                    // may be should be better to not remove the novalid references and only count the number of valid predecessors
                    for(pred_iter = appendix.predecessors->begin(); pred_iter != appendix.predecessors->end(); pred_iter++)
                    {
                      if(pred_iter->state_ref == thread_state_ref.state_ref)
                      {
                        appendix.predecessors->erase(pred_iter);
                        break;
                      }
                    }
                    if( appendix.predecessors->size() == 0 ) //indegree = 0
                    {
                      queues_iter->second->push(ref);
                      appendix.root = ref;
                      appendix.root_id = id;
                      appendix.actions = 1;
                      Storages[id]->set_app_by_ref(ref,appendix);
                      if(mode == 'a' || mode== 'd')
                      {
                        cout<<"---------------------------------------------------"<<endl;
                        cout<<"Trivial SCC found"<<endl;
                        cout<<ref<<endl;
                        cout<<"---------------------------------------------------"<<endl;
                      }
                      number_of_trivial_SCC[id]++;
                      // TO DO ref is trivial SCC should be processed
                    }
                    else
                    {
                      info_iter->second.reached->push(ref);
                    }
                  }
                }
                else // forward reachibility
                {
                  if( info_iter->second.inset == appendix.inset && appendix.visited != queues_iter->first)
                  {
                    appendix.visited = queues_iter->first;
                    appendix.inset = queues_iter->first;
                    queues_iter->second->push(ref);
                  }
                  else
                  {
                    if( info_iter->second.executed_by == appendix.inset)
                    {
                      for(pred_iter = appendix.predecessors->begin(); pred_iter != appendix.predecessors->end(); pred_iter++)
                      {
                        if(pred_iter->state_ref == thread_state_ref.state_ref)
                        {
                          appendix.predecessors->erase(pred_iter);
                          break;
                        }
                      }
                     info_iter->second.seeds->push(ref);  // FWDSEEDS found
                    }
                  }
                }
                Storages[id]->set_app_by_ref(ref,appendix); 
              }
              delete_state(remote_state.state);
            }
          }
        }
      }
      // end of reading the buffer
      waiting_status[id].value = true;
      waiting_signal = true;
      //cout<<id<<"signal send"<<endl;
     }
     if(creating_status[id].value == 1)
     {
       //deleting the empty queue
       term_queues_iter = Queues[id].find(terminating_job);
       delete(term_queues_iter->second);
       Queues[id].erase(term_queues_iter);

       if(mode == 'd') cout<<id<<"Master created the new job .... creating the new job"<<endl;
       if(terminating_job  == Working_set + 1) //terminated job is the first reachibility
       {
         //create new queue and info
         queue_pointer = new queue<state_ref_t>();
         info.reached = new queue<state_ref_t>(); 
         if(started_owcty) //started owcty
         {
           if(id == owcty_id) 
           {
             queue_pointer->push(tmp_ref);
           }
           //create the new info
           info.tag = 2;
           info.inset = Working_set;
           info.states = 0; 
           info.range = tmp_states;;
           info.executed_by = Working_set;
         }
         else //started backward reachibility
         {
           if(id == owcty_id)
           {
             info.reached->push(tmp_ref);
             queue_pointer->push(tmp_ref);
           }
           //create the new info
           info.tag = 1;
           info.inset = Working_set;
           info.states = 0; 
           info.range = tmp_states;;
           info.executed_by = Working_set;
           info.SCC_list  = new queue<state_ref_t>();
         }
         Queues[id].insert(make_pair(last_job,queue_pointer));
         Job_infos[id].insert(make_pair(last_job,info));

         //delete terminating old info
         info_iter = Job_infos[id].find(terminating_job);
         delete(info_iter->second.reached);
         Job_infos[id].erase(info_iter);
      }
      else // terminated job is not the first reachibility
      {
        switch(Job_infos[id].find(terminating_job)->second.tag)
        {
          case 0:    // terminated job is forward reachibility
            // get old info
            info_iter = Job_infos[id].find(terminating_job);

            //create new queue and info
            queue_pointer = new queue<state_ref_t>();
            info.reached = new queue<state_ref_t>(); 
            if(started_owcty) //started owcty
            {
              if(id == owcty_id) 
              {
                queue_pointer->push(info_iter->second.root);
              }
              //create the new info
              info.tag = 2;
              info.inset = terminating_job;
              info.states = 0; 
              info.range = tmp_states;
              info.executed_by = terminating_job;
            }
            else // started backward reachibility
            {
              if(id == owcty_id) 
              {
                info.reached->push(info_iter->second.root);
                queue_pointer->push(info_iter->second.root);
              }
              //create the new info
              info.tag = 1;
              info.inset = terminating_job;
              info.states = 0; 
              info.range = tmp_states;
              info.executed_by = terminating_job;
              info.SCC_list  = new queue<state_ref_t>();
            }
            if(started_fwd || seed_found)
            {
              Queues[id].insert(make_pair(last_job - 1,queue_pointer));
              Job_infos[id].insert(make_pair(last_job -1 ,info));
            }
            else
            {
              Queues[id].insert(make_pair(last_job,queue_pointer));
              Job_infos[id].insert(make_pair(last_job,info));
            }

            if(started_fwd)
            {
               info_iter = Job_infos[id].find(terminating_job);

                //create new forward reachibility
                queue_pointer = new queue<state_ref_t>();
                if(fwd_id == id)
                {
                   queue_pointer->push(tmp_ref);
                }
                Queues[id].insert(make_pair(last_job,queue_pointer));

                //create the new info for forward reachibility
                if(fwd_id == id)
                {
                   info.root = tmp_ref;
                   info.has_root = true;
                }
                else
                {
                  info.has_root = false;
                }
                info.tag = 0;
                info.inset = info_iter->second.inset;
                info.states = 0; 
                info.reached = info_iter->second.reached;
                info.range = info_iter->second.range;
                info.seeds = info_iter->second.seeds;
                info.executed_by = info_iter->second.executed_by;
                info.executed_by_BWD = info_iter->second.executed_by_BWD;
                Job_infos[id].insert(make_pair(last_job,info));
            }
            else
            {
              if(seed_found)
              {
                info_iter = Job_infos[id].find(terminating_job);

                if(mode == 'd') cout<<"Owcty from previous iteration should start"<<endl;
                //create new queue
                queue_pointer = new queue<state_ref_t>();
                info.reached = new queue<state_ref_t>();
                while( !(*info_iter->second.seeds).empty())
                {
                   my_tmp_ref = (*info_iter->second.seeds).front();
                   (*info_iter->second.seeds).pop();
                   Storages[id]->get_app_by_ref(my_tmp_ref,tmp_appendix);
                   if(tmp_appendix.visited != last_job)
                   {
                     tmp_appendix.visited = last_job;
                     if(tmp_appendix.predecessors->empty())
                     {
                       if(mode == 'd') cout<<id<<" inicial ref for previous OWCTY is "<<my_tmp_ref<<endl; 
                       queue_pointer->push(my_tmp_ref);
                       tmp_appendix.root = my_tmp_ref;
                       tmp_appendix.root_id = id;
                       tmp_appendix.actions = 1;
                       if(mode == 'a'|| mode== 'd')
                       {
                         cout<<"---------------------------------------------------"<<endl;
                         cout<<"Trivial SCC found "<<endl;
                         cout<<my_tmp_ref<<endl;
                         cout<<"---------------------------------------------------"<<endl;
                       }
                       number_of_trivial_SCC[id]++;
                     }
                     else
                     {
                       if(mode == 'd') cout<<id<<" indegree != 0 push to reached "<<my_tmp_ref<<endl; 
                       info.reached->push(my_tmp_ref);
                     }
                     Storages[id]->set_app_by_ref(my_tmp_ref,tmp_appendix);
                  }
                }
                Queues[id].insert(make_pair(last_job,queue_pointer));

                //create new info
                info.tag = 2;
                info.inset = info_iter->second.executed_by;
                info.states = 0; 
                info.range = info_iter->second.range;
                info.executed_by = info_iter->second.executed_by;
                Job_infos[id].insert(make_pair(last_job,info));
              } 
            }
            if(!started_fwd)
            {
              delete(info_iter->second.seeds);
              delete(info_iter->second.reached);
            }
            //delete terminating old info
            Job_infos[id].erase(info_iter);
            break;

          case 1:   // terminated job is backward reachibility

             info_iter = Job_infos[id].find(terminating_job);
             if(info_iter->second.range == new_range)  //All states reached in this backward reachibility forms SCC 
             {
               delete(info_iter->second.reached);
               delete(info_iter->second.SCC_list);
               Job_infos[id].erase(info_iter);
             }
             else
             {
              if(new_range != 0)
              {
                queue_pointer = new queue<state_ref_t>();

                if(started_fwd)
                {
                  if(fwd_id == id)
                  {
                    queue_pointer->push(tmp_ref);
                    info.has_root = true;
                    info.root = tmp_ref;
                  }
                  else
                  {
                    info.has_root = false;
                  }
                  //create the new queue 
                  info.tag = 0;
                  info.inset = terminating_job;
                  info.states = 0; 
                  info.reached = info_iter->second.reached;
                  info.range = info_iter->second.range;
                  info.seeds = new queue<state_ref_t>();
                  info.executed_by = info_iter->second.executed_by;
                  info.executed_by_BWD = terminating_job;
                  Job_infos[id].insert(make_pair(last_job,info));

                  //create the new queue
                  Queues[id].insert(make_pair(last_job,queue_pointer));

                  //delete SCC_list
                  delete(info_iter->second.SCC_list);
                  //delete terminating info
                  Job_infos[id].erase(info_iter);
                }
              }
             }
             break;
            case 2:  // terminated job is owcty
              // get old info
              info_iter = Job_infos[id].find(terminating_job);

              queue<state_ref_t>* tmp_queue = new queue<state_ref_t>();
              while(!(*info_iter->second.reached).empty())
              {
                my_tmp_ref = (*info_iter->second.reached).front();
                if(mode == 'd') cout<<id<<" candidate to reached state "<<my_tmp_ref<<endl;
                (*info_iter->second.reached).pop();
                Storages[id]->get_app_by_ref(my_tmp_ref,tmp_appendix);
                if(!tmp_appendix.predecessors->empty() && tmp_appendix.visited != last_job)
                {
                  tmp_appendix.visited = last_job;
                  tmp_appendix.inset = last_job;
                  Storages[id]->set_app_by_ref(my_tmp_ref,tmp_appendix);
                  tmp_queue->push(my_tmp_ref);
                  if(mode == 'd') cout<<"reached state "<<my_tmp_ref<<endl;
                }
              }
              if(mode == 'd') cout<<"Size of reached = "<<tmp_queue->size()<<endl;

              //create the new queue 
              info.tag = 1;
              info.inset = info_iter->second.inset;
              info.states = 0; 
              info.reached = tmp_queue;
              info.range = info_iter->second.range;
              info.executed_by = info_iter->second.executed_by;
              info.SCC_list  = new queue<state_ref_t>();
              Job_infos[id].insert(make_pair(last_job,info));

              //create the new queue
              queue_pointer = new queue<state_ref_t>(*(tmp_queue));
              Queues[id].insert(make_pair(last_job,queue_pointer));

              //delete terminating info
              delete(info_iter->second.reached);
              Job_infos[id].erase(info_iter);
              break;
        } //end of switch
      }
      creating_status[id].value = 2;
      if(mode == 'd') cout<<id<<"Created the new job .... waiting for unlock"<<endl;
    }
   }
    waiting_status[id].value = false;
    creating_status[id].value = 0;
   // do the jobs
   some_work = false;
   buffer_readed = false;
   n_cycles++ ;
   for(queues_iter = Queues[id].begin(); queues_iter != Queues[id].end(); queues_iter++)
   {
      //if(mode == 'd') cout<<id<<". Size of the queue "<<Queues[id].size()<<" last job = "<<last_job<<endl;
      if(! queues_iter->second->empty())
      {
        info_iter = Job_infos[id].find(queues_iter->first);  // get iterator corresponding info
        some_work = true;
        ref = queues_iter->second->front();
        if(info_iter->second.tag != 2) // forward or backward reachibility - we count the numer of visited states 
        { 
         info_iter->second.states++;
        }

        if(mode == 'd' && queues_iter->first != 0) cout<<id<<" State = "<<ref<<" tag ="<< info_iter->second.tag<<" job = "<<queues_iter->first<<endl; 

        queues_iter->second->pop();
        pred_ref = ref;
        Storages[id]->get_app_by_ref(pred_ref,pred_appendix);
        // process state
        if(info_iter->second.tag == 1) // backward reachibility
        {
          info_iter->second.SCC_list->push(pred_ref);
          for(pred_iter = pred_appendix.predecessors->begin(); pred_iter != pred_appendix.predecessors->end(); pred_iter++)
          {
            if(pred_iter->thread_id == id)
            {
              ref = pred_iter->state_ref;
              Storages[id]->get_app_by_ref(ref,appendix);
              if( info_iter->second.inset == appendix.inset && appendix.visited != queues_iter->first)
              {
                appendix.visited = queues_iter->first;
                appendix.inset = queues_iter->first;
                queues_iter->second->push(ref);
                Storages[id]->set_app_by_ref(ref,appendix);
              } 
            }
            else
            {
              // TO DO maybe should be better to check the Storages[partition_function(r)] 
              // if it is neccesery to send the state -- but not in the case of this storage implementation
              remote_state_t remote_state;
              remote_state.ref = pred_iter->state_ref;
              remote_state.job = queues_iter->first;
              remote_state.tag = info_iter->second.tag;
              Buffers[pred_iter->thread_id][id].push(remote_state);
            }
          }
        }
        else // foward reachibility or owcty
        {
          s = Storages[id]->reconstruct(ref);
          sys->get_succs_ordered_by_prob_and_property_trans(s,succs_cont);
          if(!pred_appendix.is_seed)  // no transition from seed
          for (iter_i = succs_cont.begin(); iter_i != succs_cont.end(); iter_i++)
          {
            //cout<<"successor"<<endl;
            state_t r = iter_i->state;
            if (partition_function(r) == id)
            {
               thread_state_ref.state_ref = pred_ref; 
               thread_state_ref.thread_id = id;
               if(queues_iter->first == Working_set + 1)  // the first forward reachibility
               {
                 Storages[id]->is_stored(r,ref);
                 Storages[id]->get_app_by_ref(ref,appendix);
                 if (appendix.inset == info_iter->second.inset)  
                 {
                   if(appendix.visited != queues_iter->first)
                   {
                     if(mode == 'd') cout <<"     - "<<pred_ref<<" -> "<<ref<<endl;

                     appendix.visited = queues_iter->first;
                     appendix.predecessors->clear();
                     appendix.predecessors->push_back(thread_state_ref);
                     Storages[id]->set_app_by_ref(ref,appendix); 
                     queues_iter->second->push(ref);
                   }
                   else
                   {
                     if(mode == 'd') cout <<"     - "<<pred_ref<<" -> "<<ref<<endl;

                     appendix.predecessors->push_back(thread_state_ref);
                     Storages[id]->set_app_by_ref(ref,appendix); 
                   }
                 }
               }
               else
               {
                 Storages[id]->is_stored(r,ref);
                 Storages[id]->get_app_by_ref(ref,appendix);
                 if (info_iter->second.tag == 2) // owcty
                 {
                   if(appendix.inset == info_iter->second.inset)
                   {
                     if(mode == 'd') cout <<"     - "<<pred_ref<<" -> "<<ref<<endl;
                     // may be should be better to not remove the novalid references and only count the number of valid predecessors
                     for(pred_iter = appendix.predecessors->begin(); pred_iter != appendix.predecessors->end(); pred_iter++)
                     {
                        if(pred_iter->state_ref == pred_ref)
                        {
                           appendix.predecessors->erase(pred_iter);
                           break;
                        }
                     }
                     if( appendix.predecessors->size() == 0 ) //indegree = 0
                     {
                       queues_iter->second->push(ref);
                       appendix.root = ref;
                       appendix.root_id = id;
                       appendix.actions = 1;
                       Storages[id]->set_app_by_ref(ref,appendix);
                       if(mode == 'a'|| mode== 'd')
                       {
                          cout<<"---------------------------------------------------"<<endl;
                          cout<<"Trivial SCC found"<<endl;
                          cout<<ref<<endl;
                          cout<<"---------------------------------------------------"<<endl;
                       }
                       number_of_trivial_SCC[id]++;
                       // TO DO ref is trivial SCC should be processed
                     }
                     else
                     {
                     info_iter->second.reached->push(ref);
                     }
                   }
                 }
                 else // forward reachibility 
                 {
                   if( info_iter->second.inset == appendix.inset && appendix.visited != queues_iter->first)
                   {
                     appendix.visited = queues_iter->first;
                     appendix.inset = queues_iter->first;
                     queues_iter->second->push(ref);
                   }
                   else
                   {
                     if( info_iter->second.executed_by == appendix.inset)
                     {
                       for(pred_iter = appendix.predecessors->begin(); pred_iter != appendix.predecessors->end(); pred_iter++)
                       {
                         if(pred_iter->state_ref == pred_ref)
                         {
                           appendix.predecessors->erase(pred_iter);
                           break;
                         }
                       } 
                      info_iter->second.seeds->push(ref);  // FWDSEEDS found
                     }
                   }
                 }
                 Storages[id]->set_app_by_ref(ref,appendix); 
               }
               delete_state(r);
            }
            else
            {
              // TO DO maybe should be better to check the Storages[partition_function(r)] 
              // if it is neccesery to send the state -- but not in the case of this storage implementation
              remote_state_t remote_state;
              remote_state.ref = pred_ref;
              remote_state.state = r;
              remote_state.job = queues_iter->first;
              remote_state.tag = info_iter->second.tag;
              Buffers[partition_function(r)][id].push(remote_state);
            } 
          }
          delete_state(s);
       }
     }
   }
   if( (n_cycles == WHEN_READ_FROM_BUFFER || !some_work) && Queues[id].size() != 0) //begin of reading from buffer
   {
     if(n_cycles == WHEN_READ_FROM_BUFFER) n_cycles = 0;

     buffer_readed = true;

     for(int i=0; i < Thread_count; i++)
     {
       if(i != id)
       { 
         buffer = &Buffers[id][i];
         while (!buffer->empty())
         {
           remote_state_t remote_state = buffer->front();
           buffer->pop();
           // process state
           queues_iter = Queues[id].find(remote_state.job); //get queue iterator
           info_iter = Job_infos[id].find(remote_state.job); //get info iterator
           if (remote_state.tag == 1) // backward reachibility
           {
             Storages[id]->get_app_by_ref(remote_state.ref,appendix);
             if( info_iter->second.inset == appendix.inset && appendix.visited != queues_iter->first)
             {
               appendix.visited = queues_iter->first;
               appendix.inset = queues_iter->first;
               queues_iter->second->push(remote_state.ref);
               Storages[id]->set_app_by_ref(remote_state.ref,appendix);
             } 
           }
           else // foward reachibility or owcty
           {
             thread_state_ref.state_ref = remote_state.ref; 
             thread_state_ref.thread_id = i;
             if(queues_iter->first == Working_set + 1) // the first reachibility
             {
                 Storages[id]->is_stored(remote_state.state,ref);
                 Storages[id]->get_app_by_ref(ref,appendix);
                 if (appendix.inset == info_iter->second.inset)  
                 {
                   if(appendix.visited != queues_iter->first)
                   {
                     appendix.visited = queues_iter->first;
                     appendix.predecessors->clear();
                     appendix.predecessors->push_back(thread_state_ref);
                     Storages[id]->set_app_by_ref(ref,appendix); 
                     queues_iter->second->push(ref);
                   }
                   else
                   {
                     appendix.predecessors->push_back(thread_state_ref);
                     Storages[id]->set_app_by_ref(ref,appendix); 
                   }
                 }
             }
             else
             {
               Storages[id]->is_stored(remote_state.state,ref);
               Storages[id]->get_app_by_ref(ref,appendix);
               if (remote_state.tag == 2) // owcty
               {
                 if(appendix.inset == info_iter->second.inset)
                 {
                   // may be should be better to not remove the novalid references and only count the number of valid predecessors
                   for(pred_iter = appendix.predecessors->begin(); pred_iter != appendix.predecessors->end(); pred_iter++)
                   {
                     if(pred_iter->state_ref == thread_state_ref.state_ref)
                     {
                       appendix.predecessors->erase(pred_iter);
                       break;
                     }
                   }
                   if( appendix.predecessors->size() == 0 ) //indegree = 0
                   {
                     queues_iter->second->push(ref);
                     appendix.root = ref;
                     appendix.root_id = id;
                     appendix.actions = 1;
                     Storages[id]->set_app_by_ref(ref,appendix);
                     if(mode == 'a'|| mode== 'd')
                     {
                       cout<<"---------------------------------------------------"<<endl;
                       cout<<"Trivial SCC found"<<endl;
                       cout<<ref<<endl;
                       cout<<"---------------------------------------------------"<<endl;
                     }
                     number_of_trivial_SCC[id]++;
                     // TO DO ref is trivial SCC should be processed
                   }
                   else
                   {
                      info_iter->second.reached->push(ref);
                   }
                 }
               }
               else // forward reachibility
               {
                 if( info_iter->second.inset == appendix.inset && appendix.visited != queues_iter->first)
                 {
                   appendix.visited = queues_iter->first;
                   appendix.inset = queues_iter->first;
                   queues_iter->second->push(ref);
                 }
                 else
                 {
                   if( info_iter->second.executed_by == appendix.inset)
                   {
                     for(pred_iter = appendix.predecessors->begin(); pred_iter != appendix.predecessors->end(); pred_iter++)
                     {
                       if(pred_iter->state_ref == thread_state_ref.state_ref)
                       {
                         appendix.predecessors->erase(pred_iter);
                         break;
                       }
                     }
                     info_iter->second.seeds->push(ref);  // FWDSEEDS found
                   }
                 }
               }
               Storages[id]->set_app_by_ref(ref,appendix); 
             }
             delete_state(remote_state.state);
           }
         }
       }
     }
   } //end of reading from buffer

   // termination detection
    bool terminating = false;
    int  states_in_queues = 0;

    for(queues_iter = Queues[id].begin(); (queues_iter != Queues[id].end() && !terminating && buffer_readed ); queues_iter++)
    {
       if(queues_iter->second->empty())
       {
         terminating = true;
         term_queues_iter = queues_iter;
       }
       else
       {
         states_in_queues = states_in_queues + queues_iter->second->size();
       }
    }

    queues_iter = term_queues_iter;

    if (terminating && states_in_queues < WHEN_TERMINATING)
    { 
      for(int i=0; (i < Thread_count && terminating); i++)  // check the corresponding queues system is unlocked
      {
        term_queues_iter = Queues[i].find(queues_iter->first);
        if(term_queues_iter == Queues[i].end() )
        {
           terminating = false; // job has not been inicialized yet
        }
        else
        {
          if (!term_queues_iter->second->empty() ) terminating = false;  // the queue is non-empty
        }
      }
    }


        if (terminating && Queues[id].size() != 0) // try lock evrything and try to terminate the job 
        {
           if (pthread_mutex_trylock(&mutex) == 0 )
           {
              //cout<<id<<" detecting the termination"<<endl;
              system_locked = true; // system locked - wait until others threads will wait then try to terminate the job 

              waiting_signal = false;
              while(! waiting_signal)
              {
                all_waiting = true;
                for(int i = 0; ( i < Thread_count && all_waiting); i++)
                 if ( (i != id) && (!waiting_status[i].value) ) all_waiting = false;

                if(all_waiting) waiting_signal = true; //all others threads are waiting
              }

              for(int i=0; (i < Thread_count && terminating); i++) 
              {
                if (! Queues[i].find(queues_iter->first)->second->empty() ) terminating = false  ; // check the corresponding queues system is locked
              } 
              if (terminating) //the correponding queues are empty check all buffers
              {
                for(int i=0; (i < Thread_count && terminating); i++)
                 for(int j=0; (j < Thread_count && terminating); j++)
                   if(i != j && terminating)
                   { 
                     buffer = &Buffers[i][j];  //from j to i
                     while (!buffer->empty() && terminating)
                     {
                       remote_state_t remote_state = buffer->front();
                       buffer->pop();
                       term_queues_iter = Queues[i].find(remote_state.job); // get queue iterator
                       info_iter = Job_infos[i].find(remote_state.job); //get info iterator
                       //process state
                       if (remote_state.tag == 1) //backward reachbility
                       {
                         Storages[i]->get_app_by_ref(remote_state.ref,appendix);
                         if( info_iter->second.inset == appendix.inset && appendix.visited != term_queues_iter->first)
                         {
                           appendix.visited = term_queues_iter->first;
                           appendix.inset = term_queues_iter->first;
                           term_queues_iter->second->push(remote_state.ref);
                           Storages[i]->set_app_by_ref(remote_state.ref,appendix);
                           if(queues_iter->first == remote_state.job)
                           {
                                 terminating = false;  // new state in terminating job found --> stop terminating
                           } 
                         } 
                       }
                       else // foward reachibility or seeds foward reachibility or owcty
                       {
                         thread_state_ref.state_ref = remote_state.ref; 
                         thread_state_ref.thread_id = j;
                         if(term_queues_iter->first == Working_set + 1) // the first reachibility
                         {
                           Storages[i]->is_stored(remote_state.state,ref);
                           Storages[i]->get_app_by_ref(ref,appendix);
                           if (appendix.inset == info_iter->second.inset)  
                           {
                             if(appendix.visited != term_queues_iter->first)
                             {
                               appendix.visited = term_queues_iter->first;
                               appendix.predecessors->push_back(thread_state_ref);
                               Storages[i]->set_app_by_ref(ref,appendix); 
                               term_queues_iter->second->push(ref);
                               if(queues_iter->first == remote_state.job)
                               {
                                 terminating = false;  // new state in terminating job found --> stop terminating
                               } 
                             }
                             else
                             {
                               appendix.predecessors->push_back(thread_state_ref);
                               Storages[i]->set_app_by_ref(ref,appendix); 
                             }
                           }
                         }
                         else
                         {
                           Storages[i]->is_stored(remote_state.state,ref); 
                           Storages[i]->get_app_by_ref(ref,appendix);
                           if (remote_state.tag == 2) // owcty
                           {
                             if(appendix.inset == info_iter->second.inset)
                             {
                               // may be should be better to not remove the novalid references and only count the number of valid predecessors
                               for(pred_iter = appendix.predecessors->begin(); pred_iter != appendix.predecessors->end(); pred_iter++)
                               {
                                 if(pred_iter->state_ref == thread_state_ref.state_ref)
                                 {
                                   appendix.predecessors->erase(pred_iter);
                                   break;
                                 }
                               }
                               if( appendix.predecessors->size() == 0 ) //indegree = 0
                               {
                                 term_queues_iter->second->push(ref);
                                 appendix.root = ref;
                                 appendix.root_id = i;
                                 appendix.actions = 1;
                                 Storages[i]->set_app_by_ref(ref,appendix);
                                 if(queues_iter->first == remote_state.job)
                                 {
                                   terminating = false;  // new state in terminating job found --> stop terminating
                                 } 
                                 if(mode == 'a'|| mode== 'd')
                                 {
                                   cout<<"---------------------------------------------------"<<endl;
                                   cout<<"Trivial SCC found"<<endl;
                                   cout<<ref<<endl;
                                   cout<<"---------------------------------------------------"<<endl;
                                 }
                                 number_of_trivial_SCC[i]++;
                                 // TO DO ref is trivial SCC should be processed
                               }
                               else
                               {
                                 info_iter->second.reached->push(ref);
                               }
                             }
                           }
                           else // forward reachibility
                           {
                             if(  info_iter->second.inset == appendix.inset && appendix.visited != term_queues_iter->first)
                             {
                               appendix.visited = term_queues_iter->first;
                               appendix.inset = term_queues_iter->first;
                               term_queues_iter->second->push(ref); 
                               if(queues_iter->first == remote_state.job)
                               {
                                 terminating = false;  // new state in terminating job found --> stop terminating
                               } 
                             }
                             else
                             {
                               if( info_iter->second.executed_by == appendix.inset )
                               {
                                 for(pred_iter = appendix.predecessors->begin(); pred_iter != appendix.predecessors->end(); pred_iter++)
                                 {
                                   if(pred_iter->state_ref == thread_state_ref.state_ref)
                                   {
                                     appendix.predecessors->erase(pred_iter);
                                     break;
                                   }
                                 }
                                 info_iter->second.seeds->push(ref);  // FWDSEEDS found
                               }
                             }
                           }
                           Storages[i]->set_app_by_ref(ref,appendix); 
                         }
                         delete_state(remote_state.state);
                       }
                     }
                   }
              }
              if (terminating) //the corresponding queues and all buffers are empty --> terminate a job
              {
                terminating_job = queues_iter->first;
                //deleting the empty queues

                 delete(queues_iter->second);
                 Queues[id].erase(queues_iter); //deleting empty queue in the terminating thread

                //TO DO maybe a new job should start
                tmp_states = 0;
                new_range = 0;
                started_fwd = false;
                started_owcty = false;
                seed_found = false;
                fwd_id = 0;
                owcty_id = 0;
                if(mode == 'd') cout<<id<<" Starting new job(s). Terminating job info: tag = "<<Job_infos[id].find(terminating_job)->second.tag<<" , job = "<< terminating_job<<endl;
                if(terminating_job  == Working_set +1 ) //terminated job is the first reachibility
                {
                   for(int i=0;i < Thread_count; i++)
                   {
                     info_iter = Job_infos[i].find(terminating_job);

                     tmp_states = tmp_states + info_iter->second.states;
                   }
                   // starting the new job - owcty/backward reachibility
                   last_job++;
                   s = sys->get_initial_state(); //initial state
                   owcty_id = partition_function(s);
                   Storages[owcty_id]->is_stored(s,tmp_ref);
                   Storages[owcty_id]->get_app_by_ref(tmp_ref,tmp_appendix);
                   tmp_appendix.visited = last_job;
                   if(tmp_appendix.predecessors->empty())
                   {
                     tmp_appendix.root = tmp_ref;
                     tmp_appendix.root_id = owcty_id;
                     tmp_appendix.actions = 1;
                     if(mode == 'a'|| mode== 'd')
                     {
                       cout<<"---------------------------------------------------"<<endl;
                       cout<<"Trivial SCC found"<<endl;
                       cout<<tmp_ref<<endl;
                       cout<<"---------------------------------------------------"<<endl;
                     }
                     number_of_trivial_SCC[owcty_id]++;
                     started_owcty = true;
                   }
                   Storages[owcty_id]->set_app_by_ref(tmp_ref,tmp_appendix); 
                   delete_state(s);


                     //create new queue and info
                     queue_pointer = new queue<state_ref_t>();
                     info.reached = new queue<state_ref_t>(); 
                     if(started_owcty) //started owcty
                     {
                       if(id == owcty_id) 
                       {
                         queue_pointer->push(tmp_ref);
                       }
                       //create the new info
                       info.tag = 2;
                       info.inset = Working_set;
                       info.states = 0; 
                       info.range = tmp_states;
                       info.executed_by = Working_set;
                     }
                     else //started backward reachibility
                     {
                       if(id == owcty_id)
                       {
                         info.reached->push(tmp_ref);
                         queue_pointer->push(tmp_ref);
                       }
                       //create the new info
                       info.tag = 1;
                       info.inset = Working_set;
                       info.states = 0; 
                       info.range = tmp_states;
                       info.executed_by = Working_set;
                       info.SCC_list  = new queue<state_ref_t>();
                     }
                     Queues[id].insert(make_pair(last_job,queue_pointer));
                     Job_infos[id].insert(make_pair(last_job,info));

                     //delete terminating old info
                     info_iter = Job_infos[id].find(terminating_job);
                     delete(info_iter->second.reached);
                     Job_infos[id].erase(info_iter);

                   if(mode == 'd') cout<<id<<" Job "<< last_job <<" started, tag = "<<Job_infos[id].find(last_job)->second.tag<<endl;
                }
                else // terminated job is not the first reachibility
                {
                  switch(Job_infos[id].find(terminating_job)->second.tag)
                  {
                    case 0:    // terminated job is forward reachibility
                      last_job++;
                      for(int i=0;i < Thread_count; i++)
                      {
                        info_iter = Job_infos[i].find(terminating_job);

                        tmp_states = tmp_states + info_iter->second.states;

                        if(info_iter->second.has_root)
                        {
                          owcty_id = i;
                          Storages[i]->get_app_by_ref(info_iter->second.root,tmp_appendix);
                          tmp_appendix.visited = last_job;
                          if(tmp_appendix.predecessors->empty())
                          {
                            tmp_appendix.root = info_iter->second.root;
                            tmp_appendix.root_id = i;
                            tmp_appendix.actions = 1;
                            if(mode == 'd') cout<<i<<" inicial ref for OWCTY is "<<info_iter->second.root<<endl; 
                            if(mode == 'a'|| mode== 'd')
                            {
                              cout<<"---------------------------------------------------"<<endl;
                              cout<<"Trivial SCC found "<<endl;
                              cout<<info_iter->second.root<<endl;
                              cout<<"---------------------------------------------------"<<endl;
                            }
                            number_of_trivial_SCC[i]++;
                            started_owcty = true;
                          }
                          Storages[i]->set_app_by_ref(info_iter->second.root,tmp_appendix);
                        }

                        while(!(*info_iter->second.reached).empty() && !started_fwd)
                        {
                          if(mode == 'd') cout<<"Try create new forward reachibility"<<endl;
                          tmp_ref = (*info_iter->second.reached).front();
                          (*info_iter->second.reached).pop();
                          Storages[i]->get_app_by_ref(tmp_ref,tmp_appendix);
                          if(tmp_appendix.inset == info_iter->second.executed_by_BWD)
                          {
                            started_fwd = true;
                            if(mode == 'd') cout<<i<<" inicial ref for FWD is "<<tmp_ref<<endl; 
                            tmp_appendix.visited = last_job + 1; // last_job is owcty , last_job + 1 is next forward reachibility
                            tmp_appendix.inset = last_job + 1;  // last_job is owcty , last_job + 1 is next forward reachibility
                            fwd_id = i;
                            Storages[i]->set_app_by_ref(tmp_ref,tmp_appendix);
                          }
                        }

                        if(! info_iter->second.seeds->empty())
                        {
                          seed_found = true;
                        }
                      }

                        // get old info
                        info_iter = Job_infos[id].find(terminating_job);

                        //create new queue and info
                        queue_pointer = new queue<state_ref_t>();
                        info.reached = new queue<state_ref_t>(); 
                        if(started_owcty) //started owcty
                        {
                          if(id == owcty_id) 
                          {
                            queue_pointer->push(info_iter->second.root);
                          }
                          //create the new info
                          info.tag = 2;
                          info.inset = terminating_job;
                          info.states = 0; 
                          info.range = tmp_states;
                          info.executed_by = terminating_job;
                        }
                        else // started backward reachibility
                        {
                          if(id == owcty_id) 
                          {
                            info.reached->push(info_iter->second.root);
                            queue_pointer->push(info_iter->second.root);
                          }
                          //create the new info
                          info.tag = 1;
                          info.inset = terminating_job;
                          info.states = 0; 
                          info.range = tmp_states;
                          info.executed_by = terminating_job;
                          info.SCC_list  = new queue<state_ref_t>();
                        }
                        Queues[id].insert(make_pair(last_job,queue_pointer));
                        Job_infos[id].insert(make_pair(last_job,info));

                      if(mode == 'd') cout<<id<<" Job "<< last_job <<" started, tag = "<<Job_infos[id].find(last_job)->second.tag<<endl;

                      if(started_fwd)
                      {
                        last_job++;

                          info_iter = Job_infos[id].find(terminating_job);

                          //create new forward reachibility
                          queue_pointer = new queue<state_ref_t>();
                          if(fwd_id == id)
                          {
                            queue_pointer->push(tmp_ref);
                          }
                          Queues[id].insert(make_pair(last_job,queue_pointer));

                          //create the new info for forward reachibility
                          if(fwd_id == id)
                          {
                            info.root = tmp_ref;
                            info.has_root = true;
                          }
                          else
                          {
                            info.has_root = false;
                          }
                          info.tag = 0;
                          info.inset = info_iter->second.inset;
                          info.states = 0; 
                          info.reached = info_iter->second.reached;
                          info.range = info_iter->second.range;
                          info.seeds = info_iter->second.seeds;
                          info.executed_by = info_iter->second.executed_by;
                          info.executed_by_BWD = info_iter->second.executed_by_BWD;
                          Job_infos[id].insert(make_pair(last_job,info));

                         if(mode == 'd') cout<<id<<" Job "<< last_job <<" started, tag = "<<Job_infos[id].find(last_job)->second.tag<<endl;
                      }
                      else
                      {
                        if(seed_found)
                        {
                          last_job++;

                            info_iter = Job_infos[id].find(terminating_job);

                            if(mode == 'd') cout<<"Owcty from previous iteration should start"<<endl;
                            //create new queue
                            queue_pointer = new queue<state_ref_t>();
                            info.reached = new queue<state_ref_t>();
                            while( !(*info_iter->second.seeds).empty())
                            {
                              my_tmp_ref = (*info_iter->second.seeds).front();
                              (*info_iter->second.seeds).pop();
                              Storages[id]->get_app_by_ref(my_tmp_ref,tmp_appendix);
                              if(tmp_appendix.visited != last_job)
                              {
                                tmp_appendix.visited = last_job;
                                if(tmp_appendix.predecessors->empty())
                                {
                                  if(mode == 'd') cout<<id<<" inicial ref for previous OWCTY is "<<my_tmp_ref<<endl; 
                                  queue_pointer->push(my_tmp_ref);
                                  tmp_appendix.root = my_tmp_ref;
                                  tmp_appendix.root_id = id;
                                  tmp_appendix.actions = 1;
                                  if(mode == 'a'|| mode== 'd')
                                  {
                                    cout<<"---------------------------------------------------"<<endl;
                                    cout<<"Trivial SCC found "<<endl;
                                    cout<<my_tmp_ref<<endl;
                                    cout<<"---------------------------------------------------"<<endl;
                                  }
                                  number_of_trivial_SCC[id]++;
                                }
                                else
                                {
                                  if(mode == 'd') cout<<id<<" indegree != 0 push to reached "<<my_tmp_ref<<endl; 
                                  info.reached->push(my_tmp_ref);
                                }
                                Storages[id]->set_app_by_ref(my_tmp_ref,tmp_appendix);
                              }
                            }
                            Queues[id].insert(make_pair(last_job,queue_pointer));

                            //create new info
                            info.tag = 2;
                            info.inset = info_iter->second.executed_by;
                            info.states = 0; 
                            info.range = info_iter->second.range;
                            info.executed_by = info_iter->second.executed_by;
                            Job_infos[id].insert(make_pair(last_job,info));

                          if(mode == 'd') cout<<id<<" Job "<< last_job <<" started, tag = "<<Job_infos[id].find(last_job)->second.tag<<endl;
                        } 
                      }

                        info_iter = Job_infos[id].find(terminating_job); 
                        if(!started_fwd)
                        {
                          delete(info_iter->second.seeds);
                          delete(info_iter->second.reached);
                        }
                        //delete terminating old info
                        Job_infos[id].erase(info_iter);

                      break;
                    case 1:   // terminated job is backward reachibility
                      for(int i=0;i < Thread_count; i++)
                      {
                        // get old info
                        info_iter = Job_infos[i].find(terminating_job);
                        new_range = new_range + info_iter->second.states;
                      }

                      if(mode == 'd') cout<<"old range = "<<Job_infos[id].find(terminating_job)->second.range<<" new range = "<<new_range<<endl;
                      if(Job_infos[id].find(terminating_job)->second.range == new_range)  //All states reached in this backward reachibility forms SCC 
                      {

                        if(mode == 'a'|| mode== 'd') cout<<"---------------------------------------------------"<<endl;
                        if(new_range != 1)
                        {
                          if(mode == 'a'|| mode== 'd') cout<<"Nontrivial SCC found, SCC size = "<<new_range<<endl;
                          if (new_range > max_SCC_size) max_SCC_size = new_range;
                          Number_of_nontrivial_SCC++;
                        }
                        else
                        {
                          number_of_trivial_SCC[id]++;
                          if(mode == 'a'|| mode== 'd') cout<<"Trivial SSC found by range"<<endl;
                        } 
                        //TO DO process this SCC
                        bool root_chosen = false;
                        state_ref_t scc_ref,root_ref;
                        int root_id = 0;
                        for(int i=0;i < Thread_count; i++)
                        {
                           // get old info
                           info_iter = Job_infos[i].find(terminating_job);
                           while(!(*info_iter->second.SCC_list).empty())
                           {
                             scc_ref = (*info_iter->second.SCC_list).front();
                             (*info_iter->second.SCC_list).pop();
                             if(!root_chosen)
                             {
                               root_ref = scc_ref;
                               root_id = i;
                               root_chosen = true;
                             }
                             Storages[i]->get_app_by_ref(scc_ref,tmp_appendix);
                             tmp_appendix.root = root_ref;
                             tmp_appendix.root_id = root_id;
                             tmp_appendix.actions = new_range;
                             Storages[i]->set_app_by_ref(scc_ref,tmp_appendix);
                             if(mode == 'a'|| mode== 'd') cout<<scc_ref<<endl;
                           }
                        }
                        if(mode == 'a'|| mode== 'd')  cout<<"---------------------------------------------------"<<endl;

                        //delete terminating job  info

                          info_iter = Job_infos[id].find(terminating_job);
                          delete(info_iter->second.reached);
                          delete(info_iter->second.SCC_list);
                          Job_infos[id].erase(info_iter);

                      }
                      else
                      {
                        if(new_range != 0)
                        {
                          for(int i=0;i < Thread_count; i++)
                          {
                            // get old info
                            info_iter = Job_infos[i].find(terminating_job);
                            if(!(*info_iter->second.reached).empty() && !started_fwd)
                            {
                              started_fwd = true;
                              fwd_id = i;
                              tmp_ref = (*info_iter->second.reached).front();
                              if(mode == 'd') cout<<i<<" inicial ref for FWD "<<tmp_ref<<endl; 
                              (*info_iter->second.reached).pop();
                              Storages[i]->get_app_by_ref(tmp_ref,tmp_appendix);
                              tmp_appendix.visited = last_job + 1 ;
                              tmp_appendix.inset = last_job + 1;
                              Storages[i]->set_app_by_ref(tmp_ref,tmp_appendix);
                            }
                          }
                          if(started_fwd)
                          {
                            last_job++;

                            info_iter = Job_infos[id].find(terminating_job);
                            queue_pointer = new queue<state_ref_t>();

                            if(fwd_id == id)
                            {
                              queue_pointer->push(tmp_ref);
                              info.has_root = true;
                              info.root = tmp_ref;
                            }
                            else
                            {
                              info.has_root = false;
                            }
                            //create the new queue 
                            info.tag = 0;
                            info.inset = terminating_job;
                            info.states = 0; 
                            info.reached = info_iter->second.reached;
                            info.range = info_iter->second.range;
                            info.seeds = new queue<state_ref_t>();
                            info.executed_by = info_iter->second.executed_by;
                            info.executed_by_BWD = terminating_job;
                            Job_infos[id].insert(make_pair(last_job,info));

                            //create the new queue
                            Queues[id].insert(make_pair(last_job,queue_pointer));

                            //delete SCC_list
                            delete(info_iter->second.SCC_list);
                            //delete terminating info
                            Job_infos[id].erase(info_iter);
                            if(mode == 'd') cout<<id<<" Job "<< last_job <<" started, tag = "<<Job_infos[id].find(last_job)->second.tag<<endl;
                          }
                        }
                        else
                        {
                          info_iter = Job_infos[id].find(terminating_job);
                          delete(info_iter->second.reached);
                          delete(info_iter->second.SCC_list);
                          Job_infos[id].erase(info_iter);
                        }
                      }
                      break;
                    case 2:  // terminated job is owcty
                      last_job++;

                        // get old info
                        info_iter = Job_infos[id].find(terminating_job);

                        queue<state_ref_t>* tmp_queue = new queue<state_ref_t>();
                        while(!(*info_iter->second.reached).empty())
                        {
                           my_tmp_ref = (*info_iter->second.reached).front();
                           if(mode == 'd') cout<<id<<" candidate to reached state "<<my_tmp_ref<<endl;
                           (*info_iter->second.reached).pop();
                           Storages[id]->get_app_by_ref(my_tmp_ref,tmp_appendix);
                           if(!tmp_appendix.predecessors->empty() && tmp_appendix.visited != last_job)
                           {
                              tmp_appendix.visited = last_job;
                              tmp_appendix.inset = last_job;
                              Storages[id]->set_app_by_ref(my_tmp_ref,tmp_appendix);
                              tmp_queue->push(my_tmp_ref);
                              if(mode == 'd') cout<<"reached state "<<my_tmp_ref<<endl;
                           }
                        }
                        if(mode == 'd') cout<<"Size of reached = "<<tmp_queue->size()<<endl;

                        //create the new queue 
                        info.tag = 1;
                        info.inset = info_iter->second.inset;
                        info.states = 0; 
                        info.reached = tmp_queue;
                        info.range = info_iter->second.range;
                        info.executed_by = info_iter->second.executed_by;
                        info.SCC_list  = new queue<state_ref_t>();
                        Job_infos[id].insert(make_pair(last_job,info));

                        //create the new queue
                        queue_pointer = new queue<state_ref_t>(*(tmp_queue));
                        Queues[id].insert(make_pair(last_job,queue_pointer));

                        //delete terminating info
                        delete(info_iter->second.reached);
                        Job_infos[id].erase(info_iter);

                      if(mode == 'd') cout<<id<<" Job "<< last_job <<" started, tag = "<<Job_infos[id].find(last_job)->second.tag<<endl;
                      break;
                  } //end of switch
                }
                if(mode == 'd') cout<<id<<" Master created the new job .... waiting for others"<<endl;
                for(int i=0;i < Thread_count; i++)
                {
                  creating_status[i].value = 1;
                }
                waiting_signal = false;
                while(! waiting_signal)
                {
                  all_waiting = true;
                  for(int i = 0; ( i < Thread_count && all_waiting); i++)
                   if ( (i != id) && (creating_status[i].value != 2) ) all_waiting = false;

                  if(all_waiting) waiting_signal = true; //all others threads finnished the creating of the new jobs 
                }
                if(mode == 'd') cout<<id<<" Others are ready"<<endl;
              }
              system_locked = false; //unlock the system 
              waiting_signal = false;
              while(! waiting_signal)
              {
                all_waiting = true;
                for(int i = 0; ( i < Thread_count && all_waiting); i++)
                 if ( (i != id) && (waiting_status[i].value) ) all_waiting = false;

                if(all_waiting) waiting_signal = true; //all others threads are not waiting
              }
              pthread_mutex_unlock(&mutex); // free the mutex
           }
         }
     //end of termination detection
 } //all job terminated


 info.inset = Working_set;
 s = sys->get_initial_state(); //initial state
 map<thread_state_ref_t,SCC_t*>::iterator SCC_list_iterator,SCC_list_iterator_succ;
 thread_state_ref_t SCC_id, pred_SCC_id;
 state_ref_t pred_ref_root;
 SCC_t* SCC;
 SCC_t* SCC_succ;
 SCC_t* SCC_pred;

 if (partition_function(s)== id)
 {
   last_job++;
   Storages[id]->is_stored(s,ref);
   Storages[id]->get_app_by_ref(ref,appendix);
   if(appendix.inset >= info.inset && appendix.visited != last_job)
   {
     appendix.visited = last_job;
     Storages[id]->set_app_by_ref(ref,appendix);
     SCC_id.state_ref = appendix.root;
     SCC_id.thread_id = appendix.root_id;
     SCC = new SCC_t();
     SCC->initial = true;
     SCC->root = appendix.root;
     SCC->root_id = appendix.root_id;
     SCC->size = appendix.actions;
     SCC->successors = new set<thread_state_ref_t>();
     SCC->connections = new set<thread_state_ref_t>();
     SCC->predecessors = new set<thread_state_ref_t>();
     Complete_SCC_list.insert(make_pair(SCC_id,SCC)); 
     Queues_init[id][5]->push(ref);
   }
 }
 delete_state(s);


 // Get info about trivial SCC
 if(id == 0)
 {
    for(int i=0; i < Thread_count; i++)
     Number_of_trivial_SCC = Number_of_trivial_SCC + number_of_trivial_SCC[i];
 }
 if(id == 0 && mode != 'q')
 {
   cout<<"Done. "<<endl;
   cout <<"-------------------------------------------------------------------------"<<endl;
   cout<<"time                         "<<timer.gettime()<<" s"<<endl;
   cout <<"-------------------------------------------------------------------------"<<endl;
 } 

 all_waiting = false;
 synchronization_scc[id].value = true;
 while(!all_waiting)
 {
   all_waiting = true;
   for(int i = 0; ( i < Thread_count && all_waiting); i++)
   {
     if ( !synchronization_scc[i].value )
     {
       all_waiting = false;
     }
   }
 }
 if(id == 0 && mode != 'q') 
 {
    cout<<"SCC graph building..................................................";
    cout.flush();
 }
 while(!SCC_Finnished)
 {

   waiting_signal = false;
   waiting_status[id].value = false; 

   while(system_locked)     // system is locked someone try to terminate the job --> wait
   {
     if(!waiting_signal)
     {
      // reading the buffer
     for(int i=0; i < Thread_count; i++)
     {
       if(i != id)
       { 
         buffer = &Buffers[id][i];
         while (!buffer->empty())
         {
           remote_state_t remote_state = buffer->front();
           buffer->pop();
           // process state
           queue_pointer = Queues_init[id][5]; //get queue iterator
           pred_ref_root = remote_state.ref;
           Storages[id]->is_stored(remote_state.state,ref);
           Storages[id]->get_app_by_ref(ref,appendix);
           if(appendix.inset >= info.inset)
           {
             if(mode == 'd') 
              cout<<id<<" 1 ref = "<<ref<<" root = "<<appendix.root<<" root id = "<<appendix.root_id<<" pred root = "<<pred_ref_root<<" pred root id = "<<remote_state.tag<<endl;
             if(appendix.visited != last_job)
               {
                 queue_pointer->push(ref);
                 appendix.visited = last_job;
                 Storages[id]->set_app_by_ref(ref,appendix);
                 if(appendix.root != pred_ref_root)
                 {
                   if(id == 0)
                   {
                     pred_SCC_id.state_ref = pred_ref_root;
                     pred_SCC_id.thread_id = remote_state.tag;
                     SCC_id.state_ref = appendix.root;
                     SCC_id.thread_id = appendix.root_id;

                     SCC_list_iterator = Complete_SCC_list.find(pred_SCC_id);
                     if(SCC_list_iterator == Complete_SCC_list.end())
                     {
                       SCC = new SCC_t();
                       SCC->initial = false;
                       SCC->root = pred_ref_root;
                       SCC->root_id = remote_state.tag;
                       SCC->size = remote_state.job;
                       SCC->successors = new set<thread_state_ref_t>();
                       SCC->connections = new set<thread_state_ref_t>();
                       SCC->predecessors = new set<thread_state_ref_t>();
                       SCC->successors->insert(SCC_id);
                       Complete_SCC_list.insert(make_pair(pred_SCC_id,SCC));
                     }
                     else
                     {
                       SCC_list_iterator->second->successors->insert(SCC_id);
                     }

                     SCC_list_iterator_succ = Complete_SCC_list.find(SCC_id);
                     if(SCC_list_iterator_succ == Complete_SCC_list.end())
                     {
                       SCC_succ = new SCC_t();
                       SCC_succ->initial = false;
                       SCC_succ->root = appendix.root;
                       SCC_succ->root_id = appendix.root_id;
                       SCC_succ->size = appendix.actions;
                       SCC_succ->successors = new set<thread_state_ref_t>();
                       SCC_succ->connections = new set<thread_state_ref_t>();
                       SCC_succ->predecessors = new set<thread_state_ref_t>();
                       thread_state_ref.state_ref = ref;
                       thread_state_ref.thread_id = id;
                       SCC_succ->connections->insert(thread_state_ref);
                       SCC_succ->predecessors->insert(pred_SCC_id);
                       Complete_SCC_list.insert(make_pair(SCC_id,SCC_succ));
                     }
                     else
                     {
                       SCC_succ = SCC_list_iterator_succ->second;
                       thread_state_ref.state_ref = ref;
                       thread_state_ref.thread_id = id;
                       SCC_succ->connections->insert(thread_state_ref);
                       SCC_succ->predecessors->insert(pred_SCC_id);
                     }
                   }
                   else
                   {
                     remote_SCC_t remote_SCC;
                     remote_SCC.pred_root = pred_ref_root;
                     remote_SCC.pred_root_id  = remote_state.tag;
                     remote_SCC.root = appendix.root;
                     remote_SCC.root_id = appendix.root_id;
                     remote_SCC.ref = ref;
                     remote_SCC.ref_id = id;
                     remote_SCC.size = appendix.actions;
                     remote_SCC.pred_size = remote_state.job;
                     Buffers_SCC[id].push(remote_SCC);
                   }
                 }
               }
               else
               {
                 if(appendix.root != pred_ref_root)
                 {
                   if(id == 0)
                   {
                     pred_SCC_id.state_ref = pred_ref_root;
                     pred_SCC_id.thread_id = remote_state.tag;
                     SCC_id.state_ref = appendix.root;
                     SCC_id.thread_id = appendix.root_id;

                     SCC_list_iterator = Complete_SCC_list.find(pred_SCC_id);
                     if(SCC_list_iterator == Complete_SCC_list.end())
                     {
                       SCC = new SCC_t();
                       SCC->initial = false;
                       SCC->root = pred_ref_root;
                       SCC->root_id = remote_state.tag;
                       SCC->size = remote_state.job;
                       SCC->successors = new set<thread_state_ref_t>();
                       SCC->connections = new set<thread_state_ref_t>();
                       SCC->predecessors = new set<thread_state_ref_t>();
                       SCC->successors->insert(SCC_id);
                       Complete_SCC_list.insert(make_pair(pred_SCC_id,SCC));
                     }
                     else
                     {
                       SCC_list_iterator->second->successors->insert(SCC_id);
                     }

                     SCC_list_iterator_succ = Complete_SCC_list.find(SCC_id);
                     if(SCC_list_iterator_succ == Complete_SCC_list.end())
                     {
                       SCC_succ = new SCC_t();
                       SCC_succ->initial = false;
                       SCC_succ->root = appendix.root;
                       SCC_succ->root_id = appendix.root_id;
                       SCC_succ->size = appendix.actions;
                       SCC_succ->successors = new set<thread_state_ref_t>();
                       SCC_succ->connections = new set<thread_state_ref_t>();
                       SCC_succ->predecessors = new set<thread_state_ref_t>();
                       thread_state_ref.state_ref = ref;
                       thread_state_ref.thread_id = id;
                       SCC_succ->connections->insert(thread_state_ref);
                       SCC_succ->predecessors->insert(pred_SCC_id);
                       Complete_SCC_list.insert(make_pair(SCC_id,SCC_succ));
                     }
                     else
                     {
                       SCC_succ = SCC_list_iterator_succ->second;
                       thread_state_ref.state_ref = ref;
                       thread_state_ref.thread_id = id;
                       SCC_succ->connections->insert(thread_state_ref);
                       SCC_succ->predecessors->insert(pred_SCC_id);
                     }
                   }
                   else
                   {
                     remote_SCC_t remote_SCC;
                     remote_SCC.pred_root = pred_ref_root;
                     remote_SCC.pred_root_id  = remote_state.tag;
                     remote_SCC.root = appendix.root;
                     remote_SCC.root_id = appendix.root_id;
                     remote_SCC.ref = ref;
                     remote_SCC.ref_id = id;
                     remote_SCC.size = appendix.actions;
                     remote_SCC.pred_size = remote_state.job;
                     Buffers_SCC[id].push(remote_SCC);
                   }
                 }
               }
           }
           delete_state(remote_state.state);
         }
       }
     }
      // end of reading the buffer
      waiting_status[id].value = true;
      waiting_signal = true;
      //cout<<id<<"signal send"<<endl;
     }
   }
   waiting_status[id].value = false; 

    // do the job
   some_work = false;
   buffer_readed = false;
   n_cycles++ ;
   queue_pointer = Queues_init[id][5];
   if(! queue_pointer->empty())
   {
     some_work = true;
     ref = queue_pointer->front();
     queue_pointer->pop();
     pred_ref = ref;
     Storages[id]->get_app_by_ref(pred_ref,pred_appendix);
     if(mode == 'd') cout<<id<<" State = "<<pred_ref<<" root = "<<pred_appendix.root<<" root_id = "<<pred_appendix.root_id<<endl;
     if(!pred_appendix.is_seed)  // no transition from seed
     {
       s = Storages[id]->reconstruct(ref);
       sys->get_succs_ordered_by_prob_and_property_trans(s,succs_cont);
       for (iter_i = succs_cont.begin(); iter_i != succs_cont.end(); iter_i++)
       {
          state_t r = iter_i->state;
          if (partition_function(r) == id)
          {
            Storages[id]->is_stored(r,ref);
            Storages[id]->get_app_by_ref(ref,appendix);
            if(appendix.inset >= info.inset)
            {
               if(mode == 'd')
                cout<<id<<" 2 ref = "<<ref<<" root = "<<appendix.root<<" root id = "<<appendix.root_id<<" pred root = "<<pred_appendix.root<<" pred root id = "<<pred_appendix.root_id<<endl;
               if(appendix.visited != last_job)
               {
                 queue_pointer->push(ref);
                 appendix.visited = last_job;
                 Storages[id]->set_app_by_ref(ref,appendix);
                 if(appendix.root != pred_appendix.root)
                 {
                   if(id == 0)
                   {
                     pred_SCC_id.state_ref = pred_appendix.root;
                     pred_SCC_id.thread_id = pred_appendix.root_id;
                     SCC_id.state_ref = appendix.root;
                     SCC_id.thread_id = appendix.root_id;

                     SCC_list_iterator = Complete_SCC_list.find(pred_SCC_id);
                     if(SCC_list_iterator == Complete_SCC_list.end())
                     {
                       SCC = new SCC_t();
                       SCC->initial = false;
                       SCC->root =  pred_appendix.root;
                       SCC->root_id =  pred_appendix.root_id;
                       SCC->size = pred_appendix.actions;
                       SCC->successors = new set<thread_state_ref_t>();
                       SCC->connections = new set<thread_state_ref_t>();
                       SCC->predecessors = new set<thread_state_ref_t>();
                       SCC->successors->insert(SCC_id);
                       Complete_SCC_list.insert(make_pair(pred_SCC_id,SCC));
                     }
                     else
                     {
                       SCC_list_iterator->second->successors->insert(SCC_id);
                     }

                     SCC_list_iterator_succ = Complete_SCC_list.find(SCC_id);
                     if(SCC_list_iterator_succ == Complete_SCC_list.end())
                     {
                       SCC_succ = new SCC_t();
                       SCC_succ->initial = false;
                       SCC_succ->root = appendix.root;
                       SCC_succ->root_id = appendix.root_id;
                       SCC_succ->size = appendix.actions;
                       SCC_succ->successors = new set<thread_state_ref_t>();
                       SCC_succ->connections = new set<thread_state_ref_t>();
                       SCC_succ->predecessors = new set<thread_state_ref_t>();
                       thread_state_ref.state_ref = ref;
                       thread_state_ref.thread_id = id;
                       SCC_succ->connections->insert(thread_state_ref);
                       SCC_succ->predecessors->insert(pred_SCC_id);
                       Complete_SCC_list.insert(make_pair(SCC_id,SCC_succ));
                     }
                     else
                     {
                       SCC_succ = SCC_list_iterator_succ->second;
                       thread_state_ref.state_ref = ref;
                       thread_state_ref.thread_id = id;
                       SCC_succ->connections->insert(thread_state_ref);
                       SCC_succ->predecessors->insert(pred_SCC_id);
                     }
                   }
                   else
                   {
                     remote_SCC_t remote_SCC;
                     remote_SCC.pred_root = pred_appendix.root;
                     remote_SCC.pred_root_id  = pred_appendix.root_id;
                     remote_SCC.root = appendix.root;
                     remote_SCC.root_id = appendix.root_id;
                     remote_SCC.ref = ref;
                     remote_SCC.ref_id = id;
                     remote_SCC.size = appendix.actions;
                     remote_SCC.pred_size = pred_appendix.actions;
                     Buffers_SCC[id].push(remote_SCC);
                   }
                 }
               }
               else
               {
                 if(appendix.root != pred_appendix.root)
                 {
                   if(id == 0)
                   {
                     pred_SCC_id.state_ref = pred_appendix.root;
                     pred_SCC_id.thread_id = pred_appendix.root_id;
                     SCC_id.state_ref = appendix.root;
                     SCC_id.thread_id = appendix.root_id;

                     SCC_list_iterator = Complete_SCC_list.find(pred_SCC_id);
                     if(SCC_list_iterator == Complete_SCC_list.end())
                     {
                       SCC = new SCC_t();
                       SCC->initial = false;
                       SCC->root =  pred_appendix.root;
                       SCC->root_id =  pred_appendix.root_id;
                       SCC->size = pred_appendix.actions;
                       SCC->successors = new set<thread_state_ref_t>();
                       SCC->connections = new set<thread_state_ref_t>();
                       SCC->predecessors = new set<thread_state_ref_t>();
                       SCC->successors->insert(SCC_id);
                       Complete_SCC_list.insert(make_pair(pred_SCC_id,SCC));
                     }
                     else
                     {
                       SCC_list_iterator->second->successors->insert(SCC_id);
                     }

                     SCC_list_iterator_succ = Complete_SCC_list.find(SCC_id);
                     if(SCC_list_iterator_succ == Complete_SCC_list.end())
                     {
                       SCC_succ = new SCC_t();
                       SCC_succ->initial = false;
                       SCC_succ->root = appendix.root;
                       SCC_succ->root_id = appendix.root_id;
                       SCC_succ->size = appendix.actions;
                       SCC_succ->successors = new set<thread_state_ref_t>();
                       SCC_succ->connections = new set<thread_state_ref_t>();
                       SCC_succ->predecessors = new set<thread_state_ref_t>();
                       thread_state_ref.state_ref = ref;
                       thread_state_ref.thread_id = id;
                       SCC_succ->connections->insert(thread_state_ref);
                       SCC_succ->predecessors->insert(pred_SCC_id);
                       Complete_SCC_list.insert(make_pair(SCC_id,SCC_succ));
                     }
                     else
                     {
                       SCC_succ = SCC_list_iterator_succ->second;
                       thread_state_ref.state_ref = ref;
                       thread_state_ref.thread_id = id;
                       SCC_succ->connections->insert(thread_state_ref);
                       SCC_succ->predecessors->insert(pred_SCC_id);
                     }
                   }
                   else
                   {
                     remote_SCC_t remote_SCC;
                     remote_SCC.pred_root = pred_appendix.root;
                     remote_SCC.pred_root_id  = pred_appendix.root_id;
                     remote_SCC.root = appendix.root;
                     remote_SCC.root_id = appendix.root_id;
                     remote_SCC.ref = ref;
                     remote_SCC.ref_id = id;
                     remote_SCC.size = appendix.actions;
                     remote_SCC.pred_size = pred_appendix.actions;
                     Buffers_SCC[id].push(remote_SCC);
                   }
                 }
               }
            }
            delete_state(r);
          }
          else
          {
            remote_state_t remote_state;
            remote_state.ref = pred_appendix.root;
            remote_state.state = r;
            remote_state.tag = pred_appendix.root_id;
            remote_state.job = pred_appendix.actions;
            Buffers[partition_function(r)][id].push(remote_state);
          }
       }
       delete_state(s);
     }
   }

   if(n_cycles == WHEN_READ_FROM_BUFFER || !some_work) //begin of reading from buffer
   {

     if(id == 0) // reading of SCC Buffers
     {
       for(int i=0; i < Thread_count; i++)
       {
         if(i != id)
         { 
           buffer_SCC = &Buffers_SCC[i];
           while (!buffer_SCC->empty())
           {
             remote_SCC_t remote_SCC = buffer_SCC->front();
             buffer_SCC->pop();
             pred_SCC_id.state_ref = remote_SCC.pred_root;
             pred_SCC_id.thread_id = remote_SCC.pred_root_id;
             SCC_id.state_ref = remote_SCC.root;
             SCC_id.thread_id = remote_SCC.root_id;

             SCC_list_iterator = Complete_SCC_list.find(pred_SCC_id);
             if(SCC_list_iterator == Complete_SCC_list.end())
             {
               SCC = new SCC_t();
               SCC->initial = false;
               SCC->root = remote_SCC.pred_root;
               SCC->root_id = remote_SCC.pred_root_id;
               SCC->size = remote_SCC.pred_size;
               SCC->successors = new set<thread_state_ref_t>();
               SCC->connections = new set<thread_state_ref_t>();
               SCC->predecessors = new set<thread_state_ref_t>();
               SCC->successors->insert(SCC_id);
               Complete_SCC_list.insert(make_pair(pred_SCC_id,SCC));
             }
             else
             {
               SCC_list_iterator->second->successors->insert(SCC_id);
             }

             SCC_list_iterator_succ = Complete_SCC_list.find(SCC_id);
             if(SCC_list_iterator_succ == Complete_SCC_list.end())
             {
               SCC_succ = new SCC_t();
               SCC_succ->initial = false;
               SCC_succ->root = remote_SCC.root;
               SCC_succ->root_id = remote_SCC.root_id;
               SCC_succ->size = remote_SCC.size;
               SCC_succ->successors = new set<thread_state_ref_t>();
               SCC_succ->connections = new set<thread_state_ref_t>();
               SCC_succ->predecessors = new set<thread_state_ref_t>();
               thread_state_ref.state_ref = remote_SCC.ref;
               thread_state_ref.thread_id = remote_SCC.ref_id;
               SCC_succ->connections->insert(thread_state_ref);
               SCC_succ->predecessors->insert(pred_SCC_id);
               Complete_SCC_list.insert(make_pair(SCC_id,SCC_succ));
             }
             else
             {
               SCC_succ = SCC_list_iterator_succ->second;
               thread_state_ref.state_ref = remote_SCC.ref;
               thread_state_ref.thread_id = remote_SCC.ref_id;
               SCC_succ->connections->insert(thread_state_ref);
               SCC_succ->predecessors->insert(pred_SCC_id);
             }
           }
         }
       }
     }

     if(n_cycles == WHEN_READ_FROM_BUFFER) n_cycles = 0;

     buffer_readed = true;

     for(int i=0; i < Thread_count; i++)
     {
       if(i != id)
       { 
         buffer = &Buffers[id][i];
         while (!buffer->empty())
         {
           remote_state_t remote_state = buffer->front();
           buffer->pop();
           // process state
           queue_pointer = Queues_init[id][5]; //get queue iterator
           pred_ref_root = remote_state.ref;
           Storages[id]->is_stored(remote_state.state,ref);
           Storages[id]->get_app_by_ref(ref,appendix);
           if(appendix.inset >= info.inset)
           {
             if(mode == 'd')
              cout<<id<<" 3 ref = "<<ref<<" root = "<<appendix.root<<" root id = "<<appendix.root_id<<" pred root = "<<pred_ref_root<<" pred root id = "<<remote_state.tag<<endl;
             if(appendix.visited != last_job)
               {
                 queue_pointer->push(ref);
                 appendix.visited = last_job;
                 Storages[id]->set_app_by_ref(ref,appendix);
                 if(appendix.root != pred_ref_root)
                 {
                   if(id == 0)
                   {
                     pred_SCC_id.state_ref = pred_ref_root;
                     pred_SCC_id.thread_id = remote_state.tag;
                     SCC_id.state_ref = appendix.root;
                     SCC_id.thread_id = appendix.root_id;

                     SCC_list_iterator = Complete_SCC_list.find(pred_SCC_id);
                     if(SCC_list_iterator == Complete_SCC_list.end())
                     {
                       SCC = new SCC_t();
                       SCC->initial = false;
                       SCC->root = pred_ref_root;
                       SCC->root_id = remote_state.tag;
                       SCC->size = remote_state.job;
                       SCC->successors = new set<thread_state_ref_t>();
                       SCC->connections = new set<thread_state_ref_t>();
                       SCC->predecessors = new set<thread_state_ref_t>();
                       SCC->successors->insert(SCC_id);
                       Complete_SCC_list.insert(make_pair(pred_SCC_id,SCC));
                     }
                     else
                     {
                       SCC_list_iterator->second->successors->insert(SCC_id);
                     }

                     SCC_list_iterator_succ = Complete_SCC_list.find(SCC_id);
                     if(SCC_list_iterator_succ == Complete_SCC_list.end())
                     {
                       SCC_succ = new SCC_t();
                       SCC_succ->initial = false;
                       SCC_succ->root = appendix.root;
                       SCC_succ->root_id = appendix.root_id;
                       SCC_succ->size = appendix.actions;
                       SCC_succ->successors = new set<thread_state_ref_t>();
                       SCC_succ->connections = new set<thread_state_ref_t>();
                       SCC_succ->predecessors = new set<thread_state_ref_t>();
                       thread_state_ref.state_ref = ref;
                       thread_state_ref.thread_id = id;
                       SCC_succ->connections->insert(thread_state_ref);
                       SCC_succ->predecessors->insert(pred_SCC_id);
                       Complete_SCC_list.insert(make_pair(SCC_id,SCC_succ));
                     }
                     else
                     {
                       SCC_succ = SCC_list_iterator_succ->second;
                       thread_state_ref.state_ref = ref;
                       thread_state_ref.thread_id = id;
                       SCC_succ->connections->insert(thread_state_ref);
                       SCC_succ->predecessors->insert(pred_SCC_id);
                     }
                   }
                   else
                   {
                     remote_SCC_t remote_SCC;
                     remote_SCC.pred_root = pred_ref_root;
                     remote_SCC.pred_root_id  = remote_state.tag;
                     remote_SCC.root = appendix.root;
                     remote_SCC.root_id = appendix.root_id;
                     remote_SCC.ref = ref;
                     remote_SCC.ref_id = id;
                     remote_SCC.size = appendix.actions;
                     remote_SCC.pred_size = remote_state.job;
                     Buffers_SCC[id].push(remote_SCC);
                   }
                 }
               }
               else
               {
                 if(appendix.root != pred_ref_root)
                 {
                   if(id == 0)
                   {
                     pred_SCC_id.state_ref = pred_ref_root;
                     pred_SCC_id.thread_id = remote_state.tag;
                     SCC_id.state_ref = appendix.root;
                     SCC_id.thread_id = appendix.root_id;

                     SCC_list_iterator = Complete_SCC_list.find(pred_SCC_id);
                     if(SCC_list_iterator == Complete_SCC_list.end())
                     {
                       SCC = new SCC_t();
                       SCC->initial = false;
                       SCC->root = pred_ref_root;
                       SCC->root_id = remote_state.tag;
                       SCC->size = remote_state.job;
                       SCC->successors = new set<thread_state_ref_t>();
                       SCC->connections = new set<thread_state_ref_t>();
                       SCC->predecessors = new set<thread_state_ref_t>();
                       SCC->successors->insert(SCC_id);
                       Complete_SCC_list.insert(make_pair(pred_SCC_id,SCC));
                     }
                     else
                     {
                       SCC_list_iterator->second->successors->insert(SCC_id);
                     }

                     SCC_list_iterator_succ = Complete_SCC_list.find(SCC_id);
                     if(SCC_list_iterator_succ == Complete_SCC_list.end())
                     {
                       SCC_succ = new SCC_t();
                       SCC_succ->initial = false;
                       SCC_succ->root = appendix.root;
                       SCC_succ->root_id = appendix.root_id;
                       SCC_succ->size = appendix.actions;
                       SCC_succ->successors = new set<thread_state_ref_t>();
                       SCC_succ->connections = new set<thread_state_ref_t>();
                       SCC_succ->predecessors = new set<thread_state_ref_t>();
                       thread_state_ref.state_ref = ref;
                       thread_state_ref.thread_id = id;
                       SCC_succ->connections->insert(thread_state_ref);
                       SCC_succ->predecessors->insert(pred_SCC_id);
                       Complete_SCC_list.insert(make_pair(SCC_id,SCC_succ));
                     }
                     else
                     {
                       SCC_succ = SCC_list_iterator_succ->second;
                       thread_state_ref.state_ref = ref;
                       thread_state_ref.thread_id = id;
                       SCC_succ->connections->insert(thread_state_ref);
                       SCC_succ->predecessors->insert(pred_SCC_id);
                     }
                   }
                   else
                   {
                     remote_SCC_t remote_SCC;
                     remote_SCC.pred_root = pred_ref_root;
                     remote_SCC.pred_root_id  = remote_state.tag;
                     remote_SCC.root = appendix.root;
                     remote_SCC.root_id = appendix.root_id;
                     remote_SCC.ref = ref;
                     remote_SCC.ref_id = id;
                     remote_SCC.size = appendix.actions;
                     remote_SCC.pred_size = remote_state.job;
                     Buffers_SCC[id].push(remote_SCC);
                   }
                 }
               }
           }
           delete_state(remote_state.state);
         }
       }
     }
   }

    bool terminating = false;
    if (buffer_readed)
    {
       if(Queues_init[id][5]->empty())
       {
         terminating = true;
       }
    }


    if (terminating)
    { 
      for(int i=0; (i < Thread_count && terminating); i++)  // check the corresponding queues system is unlocked
      {
        if (!Queues_init[i][5]->empty() ) terminating = false;  // the queue is non-empty
      }
    }

        if (terminating && !SCC_Finnished) // try lock evrything and try to terminate the job 
        {
           if (pthread_mutex_trylock(&mutex) == 0 )
           {
              //cout<<id<<" detecting the termination"<<endl;
              system_locked = true; // system locked - wait until others threads will wait then try to terminate the job 

              waiting_signal = false;
              while(! waiting_signal)
              {
                all_waiting = true;
                for(int i = 0; ( i < Thread_count && all_waiting); i++)
                 if ( (i != id) && (!waiting_status[i].value) ) all_waiting = false;

                if(all_waiting) waiting_signal = true; //all others threads are waiting
              }

              for(int i=0; (i < Thread_count && terminating); i++) 
              {
                if (! Queues_init[i][5]->empty() ) terminating = false  ; // check the corresponding queues system is locked
              } 
              if (terminating) //the correponding queues are empty check all buffers
              {
                for(int i=0; (i < Thread_count && terminating); i++)
                 for(int j=0; (j < Thread_count && terminating); j++)
                   if(i != j && terminating)
                   { 
                     buffer = &Buffers[i][j];  //from j to i
                     while (!buffer->empty() && terminating)
                     {
                       remote_state_t remote_state = buffer->front();
                       buffer->pop();
                       // process state
                       queue_pointer = Queues_init[i][5]; //get queue iterator
                       pred_ref_root = remote_state.ref;
                       Storages[i]->is_stored(remote_state.state,ref);
                       Storages[i]->get_app_by_ref(ref,appendix);
                       if(appendix.inset >= info.inset)
                       {
                         if(mode == 'd')
                          cout<<i<<" 4 ref = "<<ref<<" root = "<<appendix.root<<" root id = "<<appendix.root_id<<" pred root = "<<pred_ref_root<<" pred root id = "<<remote_state.tag<<endl;
                         if(appendix.visited != last_job)
                         {
                           queue_pointer->push(ref);
                           terminating = false;
                           appendix.visited = last_job;
                           Storages[i]->set_app_by_ref(ref,appendix);
                           if(appendix.root != pred_ref_root)
                           {
                             if(i == 0)
                             {
                               pred_SCC_id.state_ref = pred_ref_root;
                               pred_SCC_id.thread_id = remote_state.tag;
                               SCC_id.state_ref = appendix.root;
                               SCC_id.thread_id = appendix.root_id; 

                               SCC_list_iterator = Complete_SCC_list.find(pred_SCC_id);
                               if(SCC_list_iterator == Complete_SCC_list.end())
                               {
                                 SCC = new SCC_t();
                                 SCC->initial = false;
                                 SCC->root = pred_ref_root;
                                 SCC->root_id = remote_state.tag;
                                 SCC->size = remote_state.job;
                                 SCC->successors = new set<thread_state_ref_t>();
                                 SCC->connections = new set<thread_state_ref_t>();
                                 SCC->predecessors = new set<thread_state_ref_t>();
                                 SCC->successors->insert(SCC_id);
                                 Complete_SCC_list.insert(make_pair(pred_SCC_id,SCC));
                               }
                               else
                               {
                                 SCC_list_iterator->second->successors->insert(SCC_id);
                               }

                               SCC_list_iterator_succ = Complete_SCC_list.find(SCC_id);
                               if(SCC_list_iterator_succ == Complete_SCC_list.end())
                               {
                                 SCC_succ = new SCC_t();
                                 SCC_succ->initial = false;
                                 SCC_succ->root = appendix.root;
                                 SCC_succ->root_id = appendix.root_id;
                                 SCC_succ->size = appendix.actions;
                                 SCC_succ->successors = new set<thread_state_ref_t>();
                                 SCC_succ->connections = new set<thread_state_ref_t>();
                                 SCC_succ->predecessors = new set<thread_state_ref_t>();
                                 thread_state_ref.state_ref = ref;
                                 thread_state_ref.thread_id = i;
                                 SCC_succ->connections->insert(thread_state_ref);
                                 SCC_succ->predecessors->insert(pred_SCC_id);
                                 Complete_SCC_list.insert(make_pair(SCC_id,SCC_succ));
                               }
                               else
                               {
                                 SCC_succ = SCC_list_iterator_succ->second;
                                 thread_state_ref.state_ref = ref;
                                 thread_state_ref.thread_id = i;
                                 SCC_succ->connections->insert(thread_state_ref);
                                 SCC_succ->predecessors->insert(pred_SCC_id);
                               }
                             }
                             else
                             {
                                remote_SCC_t remote_SCC;
                                remote_SCC.pred_root = pred_ref_root;
                                remote_SCC.pred_root_id  = remote_state.tag;
                                remote_SCC.root = appendix.root;
                                remote_SCC.root_id = appendix.root_id;
                                remote_SCC.ref = ref;
                                remote_SCC.ref_id = i;
                                remote_SCC.size = appendix.actions;
                                remote_SCC.pred_size = remote_state.job;
                                Buffers_SCC[i].push(remote_SCC);
                             }
                           }
                         }
                         else
                         {
                           if(appendix.root != pred_ref_root)
                           {
                             if(i == 0)
                             {
                               pred_SCC_id.state_ref = pred_ref_root;
                               pred_SCC_id.thread_id = remote_state.tag;
                               SCC_id.state_ref = appendix.root;
                               SCC_id.thread_id = appendix.root_id;

                               SCC_list_iterator = Complete_SCC_list.find(pred_SCC_id);
                               if(SCC_list_iterator == Complete_SCC_list.end())
                               {
                                 SCC = new SCC_t();
                                 SCC->initial = false;
                                 SCC->root = pred_ref_root;
                                 SCC->root_id = remote_state.tag;
                                 SCC->size = remote_state.job;
                                 SCC->successors = new set<thread_state_ref_t>();
                                 SCC->connections = new set<thread_state_ref_t>();
                                 SCC->predecessors = new set<thread_state_ref_t>();
                                 SCC->successors->insert(SCC_id);
                                 Complete_SCC_list.insert(make_pair(pred_SCC_id,SCC));
                               }
                               else
                               {
                                 SCC_list_iterator->second->successors->insert(SCC_id);
                               }

                               SCC_list_iterator_succ = Complete_SCC_list.find(SCC_id);
                               if(SCC_list_iterator_succ == Complete_SCC_list.end())
                               {
                                 SCC_succ = new SCC_t();
                                 SCC_succ->initial = false;
                                 SCC_succ->root = appendix.root;
                                 SCC_succ->root_id = appendix.root_id;
                                 SCC_succ->size = appendix.actions;
                                 SCC_succ->successors = new set<thread_state_ref_t>();
                                 SCC_succ->connections = new set<thread_state_ref_t>();
                                 SCC_succ->predecessors = new set<thread_state_ref_t>();
                                 thread_state_ref.state_ref = ref;
                                 thread_state_ref.thread_id = i;
                                 SCC_succ->connections->insert(thread_state_ref);
                                 SCC_succ->predecessors->insert(pred_SCC_id);
                                 Complete_SCC_list.insert(make_pair(SCC_id,SCC_succ));
                               }
                               else
                               {
                                 SCC_succ = SCC_list_iterator_succ->second;
                                 thread_state_ref.state_ref = ref;
                                 thread_state_ref.thread_id = i;
                                 SCC_succ->connections->insert(thread_state_ref);
                                 SCC_succ->predecessors->insert(pred_SCC_id);
                               }
                             }
                             else
                             {
                                remote_SCC_t remote_SCC;
                                remote_SCC.pred_root = pred_ref_root;
                                remote_SCC.pred_root_id  = remote_state.tag;
                                remote_SCC.root = appendix.root;
                                remote_SCC.root_id = appendix.root_id;
                                remote_SCC.ref = ref;
                                remote_SCC.ref_id = i;
                                remote_SCC.size = appendix.actions;
                                remote_SCC.pred_size = remote_state.job;
                                Buffers_SCC[i].push(remote_SCC);
                             }
                           }
                         }
                       }
                       delete_state(remote_state.state);
                     }
                  }
              }
              if (terminating)
              {
                SCC_Finnished = true;
              }
              system_locked = false; //unlock the system 
              waiting_signal = false;
              while(! waiting_signal)
              {
                all_waiting = true;
                for(int i = 0; ( i < Thread_count && all_waiting); i++)
                 if ( (i != id) && (waiting_status[i].value) ) all_waiting = false;

                if(all_waiting) waiting_signal = true; //all others threads are not waiting
              }
              pthread_mutex_unlock(&mutex); // free the mutex
           }
        }
 }

all_waiting = false;
synchronization_scc_graph[id].value = true;
while(!all_waiting)
{
  all_waiting = true;
  for(int i = 0; ( i < Thread_count && all_waiting); i++)
  {
    if ( !synchronization_scc_graph[i].value )
    {
       all_waiting = false;
    }
  }
}

    if(id == 0) // finnish the reading of SCC Buffers
     {
       for(int i=0; i < Thread_count; i++)
       {
         if(i != id)
         { 
           buffer_SCC = &Buffers_SCC[i];
           while (!buffer_SCC->empty())
           {
             remote_SCC_t remote_SCC = buffer_SCC->front();
             buffer_SCC->pop();
             pred_SCC_id.state_ref = remote_SCC.pred_root;
             pred_SCC_id.thread_id = remote_SCC.pred_root_id;
             SCC_id.state_ref = remote_SCC.root;
             SCC_id.thread_id = remote_SCC.root_id;

             SCC_list_iterator = Complete_SCC_list.find(pred_SCC_id);
             if(SCC_list_iterator == Complete_SCC_list.end())
             {
               SCC = new SCC_t();
               SCC->initial = false;
               SCC->root = remote_SCC.pred_root;
               SCC->root_id = remote_SCC.pred_root_id;
               SCC->size = remote_SCC.pred_size;
               SCC->successors = new set<thread_state_ref_t>();
               SCC->connections = new set<thread_state_ref_t>();
               SCC->predecessors = new set<thread_state_ref_t>();
               SCC->successors->insert(SCC_id);
               Complete_SCC_list.insert(make_pair(pred_SCC_id,SCC));
             }
             else
             {
               SCC_list_iterator->second->successors->insert(SCC_id);
             }

             SCC_list_iterator_succ = Complete_SCC_list.find(SCC_id);
             if(SCC_list_iterator_succ == Complete_SCC_list.end())
             {
               SCC_succ = new SCC_t();
               SCC_succ->initial = false;
               SCC_succ->root = remote_SCC.root;
               SCC_succ->root_id = remote_SCC.root_id;
               SCC_succ->size = remote_SCC.size;
               SCC_succ->successors = new set<thread_state_ref_t>();
               SCC_succ->connections = new set<thread_state_ref_t>();
               SCC_succ->predecessors = new set<thread_state_ref_t>();
               thread_state_ref.state_ref = remote_SCC.ref;
               thread_state_ref.thread_id = remote_SCC.ref_id;
               SCC_succ->connections->insert(thread_state_ref);
               SCC_succ->predecessors->insert(pred_SCC_id);
               Complete_SCC_list.insert(make_pair(SCC_id,SCC_succ));
             }
             else
             {
               SCC_succ = SCC_list_iterator_succ->second;
               thread_state_ref.state_ref = remote_SCC.ref;
               thread_state_ref.thread_id = remote_SCC.ref_id;
               SCC_succ->connections->insert(thread_state_ref);
               SCC_succ->predecessors->insert(pred_SCC_id);
             }
           }
         }
       }
       for(map<thread_state_ref_t,SCC_t*>::iterator iterator = Complete_SCC_list.begin(); iterator != Complete_SCC_list.end(); iterator++)
       {
         if(iterator->second->successors->empty() ) Terminal_SCC_list.push(iterator->first);
         iterator->second->succs = iterator->second->successors->size();
         if( mode == 'd'||mode == 'a' )
         {
           cout<<"---------------------------------------------------"<<endl;
           cout<<"SCC root:                   "<<iterator->second->root<<endl;
           cout<<"SCC size:                   "<<iterator->second->size<<endl;
           cout<<"Number of successors:       "<<iterator->second->succs<<endl;
           cout<<"Number of predecessors:     "<<iterator->second->predecessors->size()<<endl;
           cout<<"---------------------------------------------------"<<endl;
        }
      }
   }


if(id == 0 && mode != 'q')
 {
   cout<<"Done. "<<endl; 
   cout <<"-------------------------------------------------------------------------"<<endl;
   cout <<"number of SCC:               "<<Complete_SCC_list.size()<<endl;
   cout <<"number of trivial SCC:       "<<Number_of_trivial_SCC<<endl;
   cout <<"number of nontrivial SCC:    "<<Number_of_nontrivial_SCC<<endl;
   cout <<"number of terminal SCC:      "<<Terminal_SCC_list.size()<<endl;
   cout <<"max size of SCC:             "<<max_SCC_size<<endl;
   cout <<"time                         "<<timer.gettime()<<" s"<<endl;
   cout <<"SCC time                     "<<scc_timer.gettime()<<" s"<<endl;
   cout <<"-------------------------------------------------------------------------"<<endl;
   cout <<"Linear programming problem solving..................................";
   cout.flush();
 }

if(id == 0)
{
lp_timer.reset();
}

set<thread_state_ref_t>::iterator thread_set_iter;

 if(Thread_count == 1)
 {
   state_ref_t root;
   while(!Terminal_SCC_list.empty()&& ! Solved)
   {
        SCC_id = Terminal_SCC_list.front();
        Terminal_SCC_list.pop();
        SCC_list_iterator = Complete_SCC_list.find(SCC_id);
        SCC = SCC_list_iterator->second;
        if(SCC->size == 1)
        {
          Storages[SCC->root_id]->get_app_by_ref(SCC->root,appendix);
          if(appendix.is_seed)
          {
             appendix.value = 1;
             Storages[SCC->root_id]->set_app_by_ref(SCC->root,appendix);
             number_of_triv_equations[id]++;
             //cout<<"seed = "<<SCC->root<<" value = "<<appendix.value<<" size of pred = "<<SCC_list_iterator->second->predecessors->size()<<endl;
          }
          else
          {
            Storages[SCC->root_id]->get_app_by_ref(SCC->root,pred_appendix);
            s = Storages[SCC->root_id]->reconstruct(SCC->root);
            //cout<<"root = "<<SCC->root<<endl;
            sys->get_succs_ordered_by_prob_and_property_trans(s,succs_cont);
            iter_i = succs_cont.begin(); 
            while(iter_i != succs_cont.end())
            {
              state_t r = (*iter_i).state;
              Storages[partition_function(r)]->is_stored(r,ref);
              Storages[partition_function(r)]->get_app_by_ref(ref,appendix);
              if ( (int)iter_i->prob_and_property_trans.prob_trans_gid == -1) // non-probabilistic
              {
                if( (appendix.inset >= info.inset || appendix.inset == -3) && (SCC->root != ref) )
                { 
                  //cout<<"succ = "<<ref<<" value = "<<appendix.value<<endl;
                  if(pred_appendix.value < appendix.value) pred_appendix.value = appendix.value;
                  number_of_triv_equations[id]++;
                }
                delete_state(r);
                iter_i++;
              }
              else
              {
                long double right_value = 0;
                long double koef = 0;
                iter_j = iter_i;
                state_ref_t ref_i = ref;

                if(ref == SCC->root) //probabilisitc self loop
                {
                  koef = koef + (((long double)iter_i->weight)/((long double)iter_i->sum));
                }
                prob_and_property_trans_t previous_action;
                previous_action = ((*iter_i).prob_and_property_trans);
                iter_i++;
                while (iter_i != succs_cont.end() && previous_action == ((*iter_i).prob_and_property_trans))
                {
                  state_t r = (*iter_i).state;
                  Storages[partition_function(r)]->is_stored(r,ref);
                  if(ref == SCC->root) //probabilisitc self loop
                  {
                    koef = koef + (((long double)iter_i->weight)/((long double)iter_i->sum));
                  }
                  iter_i++;
                }

                iter_i = iter_j;
                koef = 1.0 - koef;


                if((appendix.inset >= info.inset || appendix.inset == -3) && (SCC->root != ref_i))
                {  
                 //cout<<"prob succ = "<<ref<<" value = "<<appendix.value<<endl;
                 right_value = right_value + appendix.value*(((long double)iter_i->weight)/((long double)iter_i->sum)/koef);
                }
                delete_state(r);
                iter_i++;
                while (iter_i != succs_cont.end() && previous_action == ((*iter_i).prob_and_property_trans))
                {
                  state_t r = (*iter_i).state;
                  Storages[partition_function(r)]->is_stored(r,ref);
                  Storages[partition_function(r)]->get_app_by_ref(ref,appendix);
                  if( (appendix.inset >= info.inset || appendix.inset == -3) && (SCC->root != ref) )
                  { 
                   //cout<<"prob succ = "<<ref<<" value = "<<appendix.value<<endl; 
                   right_value = right_value + appendix.value*(((long double)iter_i->weight)/((long double)iter_i->sum)/koef);
                  }
                  delete_state(r);
                  iter_i++;
                }
                if(pred_appendix.value < right_value) pred_appendix.value = right_value;
                number_of_triv_equations[id]++;
              }
            }
            delete_state(s);
            if(SCC->initial)
            {
              Solved = true;
              Result =  pred_appendix.value;
            }
            Storages[SCC->root_id]->set_app_by_ref(SCC->root,pred_appendix);
            if( mode == 'd'||mode == 'a' ) cout<<SCC->root<<" Trivial component. End value = "<<pred_appendix.value<<endl;
          }
          if(!Solved)
          {
            for(thread_set_iter = SCC_list_iterator->second->predecessors->begin(); thread_set_iter != SCC_list_iterator->second->predecessors->end(); thread_set_iter++)
            {
              SCC_pred = Complete_SCC_list.find(*thread_set_iter)->second;
              SCC_pred->succs--;
              //cout<<"pred 1 = "<<thread_set_iter->state_ref<<" succs = "<<SCC_pred->succs<<endl;
              if(SCC_pred->succs == 0 )
              {
                 SCC_id.state_ref = SCC_pred->root;
                 SCC_id.thread_id = SCC_pred->root_id;
                 Terminal_SCC_list.push(SCC_id);
              }
            }
            Complete_SCC_list.erase(SCC_list_iterator);
          }
        }
        else
        {
          thread_state_ref.thread_id = SCC->root_id;
          thread_state_ref.state_ref = SCC->root;
          Storages[thread_state_ref.thread_id]->get_app_by_ref(thread_state_ref.state_ref,appendix);
          appendix.visited = 1;
          appendix.inset = -3;
          Storages[thread_state_ref.thread_id]->set_app_by_ref(thread_state_ref.state_ref,appendix);
          LP_queues[id]->push(thread_state_ref);

          //cout<<"Starting SCC size = "<<SCC->size<<" root = "<<SCC->root<<" connections = "<<SCC->connections->size()<<endl;
          thread_state_ref_t succ_thread_state_ref;
          lprec *lp;
          int *colno = NULL;
          REAL *row = NULL;
          colno = (int *) malloc(SCC->size * sizeof(*colno));
          row = (REAL *) malloc(SCC->size * sizeof(*row));
          int lp_index = 0;
          int variable_id = 1;
          lp = make_lp(0, SCC->size);
          set_add_rowmode(lp, TRUE);
          while(!LP_queues[id]->empty())
          {
            thread_state_ref = LP_queues[id]->front();
            LP_queues[id]->pop();
            Storages[thread_state_ref.thread_id]->get_app_by_ref(thread_state_ref.state_ref,pred_appendix);
            if(thread_state_ref.state_ref == initial_ref)
            {
               initial_visited = pred_appendix.visited;
            }
            s = Storages[thread_state_ref.thread_id]->reconstruct(thread_state_ref.state_ref);
            sys->get_succs_ordered_by_prob_and_property_trans(s,succs_cont);
            iter_i = succs_cont.begin();
            while(iter_i != succs_cont.end())
            {
              state_t r = (*iter_i).state;
              Storages[partition_function(r)]->is_stored(r,ref);
              Storages[partition_function(r)]->get_app_by_ref(ref,appendix);
              if ( (int)iter_i->prob_and_property_trans.prob_trans_gid == -1) // non-probabilistic
              {
                if(appendix.inset >= info.inset || appendix.inset == -3) // in this SCC generate equations
                {
                   if(appendix.root == pred_appendix.root)
                   {
                     if(appendix.inset != -3)
                     {
                       appendix.inset = -3;
                       succ_thread_state_ref.thread_id = partition_function(r);
                       succ_thread_state_ref.state_ref = ref;
                       LP_queues[id]->push(succ_thread_state_ref);
                       variable_id++;
                       appendix.visited = variable_id;
                       Storages[partition_function(r)]->set_app_by_ref(ref,appendix);
                     }
                     if(appendix.visited != pred_appendix.visited)
                     {
                       lp_index = 0;
                       colno[lp_index] = pred_appendix.visited; /* first column */
                       row[lp_index++] = 1;
                       colno[lp_index] = appendix.visited; /* second column */
                       row[lp_index++] = -1;
                       add_constraintex(lp, lp_index, row, colno, GE, 0);
                       number_of_equations[id]++;
                     }
                  }
                  else // not in this SCC generate equations
                  {
                    lp_index = 0;
                    colno[lp_index] = pred_appendix.visited; /* first column */
                    row[lp_index++] = 1;
                    add_constraintex(lp, lp_index, row, colno, GE, appendix.value);
                    number_of_equations[id]++;
                  }
                }
                delete_state(r);
                iter_i++;
             }
             else // probabilistic
             {
               long double right_value = 0;
               lp_index = 0;
               colno[lp_index] = pred_appendix.visited; /* first column */
               row[lp_index++] = 1;
               if(appendix.inset >= info.inset || appendix.inset == -3) // in this SCC generate equations
               {
                 if(appendix.root == pred_appendix.root)
                 {
                   if(appendix.inset != -3)
                   {
                     appendix.inset = -3;
                     succ_thread_state_ref.thread_id = partition_function(r);
                     succ_thread_state_ref.state_ref = ref;
                     LP_queues[id]->push(succ_thread_state_ref);
                     variable_id++;
                     appendix.visited = variable_id;
                     Storages[partition_function(r)]->set_app_by_ref(ref,appendix);
                   }
                   colno[lp_index] = appendix.visited; /* second column */
                   row[lp_index++] = -1*(((long double)iter_i->weight)/((long double)iter_i->sum));
                 }
                 else // not in this SCC generate equations
                 {
                   right_value = right_value + appendix.value*(((long double)iter_i->weight)/((long double)iter_i->sum));
                 }
               }
               delete_state(r);
               prob_and_property_trans_t previous_action;
               previous_action = ((*iter_i).prob_and_property_trans);
               iter_i++;
               while (iter_i != succs_cont.end() && previous_action == ((*iter_i).prob_and_property_trans))
               {
                  state_t r = (*iter_i).state;
                  Storages[partition_function(r)]->is_stored(r,ref);
                  Storages[partition_function(r)]->get_app_by_ref(ref,appendix);
                  if(appendix.inset >= info.inset || appendix.inset == -3) // in this SCC generate equations
                  {
                    if(appendix.root == pred_appendix.root)
                    {
                      if(appendix.inset != -3)
                      {
                        appendix.inset = -3;
                        succ_thread_state_ref.thread_id = partition_function(r);
                        succ_thread_state_ref.state_ref = ref;
                        LP_queues[id]->push(succ_thread_state_ref);
                        variable_id++;
                        appendix.visited = variable_id;
                        Storages[partition_function(r)]->set_app_by_ref(ref,appendix);
                      }
                      colno[lp_index] = appendix.visited; /* second column */
                      row[lp_index++] = -1*(((long double)iter_i->weight)/((long double)iter_i->sum));
                    }
                    else // not in this SCC generate equations
                    {
                      right_value = right_value + appendix.value*(((long double)iter_i->weight)/((long double)iter_i->sum));
                    }
                  }
                  delete_state(r);
                  iter_i++;
               }
               add_constraintex(lp, lp_index, row, colno, GE, right_value);
               number_of_equations[id]++;
             }
            }
            delete_state(s);
          }
          set_add_rowmode(lp, FALSE);
          if(SCC->initial)
          {
            lp_index = 0;
            colno[lp_index] = initial_visited; 
            row[lp_index++] = 1;
          }
          else
          {
            for(thread_set_iter = SCC->connections->begin(); thread_set_iter != SCC->connections->end(); thread_set_iter++)
            {
              Storages[thread_set_iter->thread_id]->get_app_by_ref(thread_set_iter->state_ref,appendix);
              lp_index = 0;
              colno[lp_index] = appendix.visited; 
              row[lp_index++] = 1;
            }
          }
          set_obj_fnex(lp, lp_index, row, colno);
          set_minim(lp);
          set_simplextype(lp, SIMPLEX_DUAL_DUAL);
          //write_LP(lp, stdout);
          set_verbose(lp, CRITICAL);
          if( mode == 'd'||mode == 'a' ) cout<<"Solving SCC of size = "<<SCC->size<<" ...."<<endl;
          solve(lp);
          if( mode == 'd'||mode == 'a' ) cout<<"SCC of size = "<<SCC->size<<" Solved"<<endl;
          get_variables(lp, row);
          //for(lp_index = 0; lp_index < SCC->size; lp_index++)
          //cout<<"Variable "<<lp_index + 1<<" =  "<<row[lp_index]<<endl;
          if(SCC->initial)
          {
            Result = row[initial_visited - 1 ]; 
          }
          else
          {
            for(thread_set_iter = SCC->connections->begin(); thread_set_iter != SCC->connections->end(); thread_set_iter++)
            {
               Storages[thread_set_iter->thread_id]->get_app_by_ref(thread_set_iter->state_ref,appendix);
               appendix.value = row[appendix.visited - 1 ];
               if( mode == 'd'||mode == 'a' ) cout<<thread_set_iter->state_ref<<"Nontrivial component End value = "<<appendix.value<<endl;
               Storages[thread_set_iter->thread_id]->set_app_by_ref(thread_set_iter->state_ref,appendix);
            }
          }
          if(row != NULL) free(row);
          if(colno != NULL) free(colno);
          if(lp != NULL) delete_lp(lp);

          if(!SCC->initial)
          {
            for(thread_set_iter = SCC_list_iterator->second->predecessors->begin(); thread_set_iter != SCC_list_iterator->second->predecessors->end(); thread_set_iter++)
            {
              SCC_pred = Complete_SCC_list.find(*thread_set_iter)->second;
              SCC_pred->succs--;
              //cout<<"pred 1.1 = "<<thread_set_iter->state_ref<<" succs = "<<SCC_pred->succs<<endl;
              if(SCC_pred->succs == 0 )
              {
                 SCC_id.state_ref = SCC_pred->root;
                 SCC_id.thread_id = SCC_pred->root_id;
                 Terminal_SCC_list.push(SCC_id);
              }
            }
            Complete_SCC_list.erase(SCC_list_iterator);
         }
     }
   }
 }
 else
 {
  while(!Solved)
  {
   if(id == 0)
   {
      if(finnishing_job_started)
      {
         while(has_job[finnishing_id].value != 2)
         {
           ;
         }
         Solved = true;
      }
      state_ref_t root;
      if(!Terminal_SCC_list.empty())
      {
        SCC_id = Terminal_SCC_list.front();
        Terminal_SCC_list.pop();
        SCC_list_iterator = Complete_SCC_list.find(SCC_id);
        SCC = SCC_list_iterator->second;
        bool job_sent = false;
        int ii;
        while(!job_sent && !Solved)
        {
          for(ii = 1; ii < Thread_count && !job_sent; ii++)
          {
            if(has_job[ii].value == 0)
            {
              job_sent = true;
              if(SCC->initial)
              {
                 finnishing_id = ii;
                 finnishing_job_started = true;
              }
              SCC_pointers[ii] = SCC;
              if(SCC->size != 1)
              {
                thread_state_ref.thread_id = SCC->root_id;
                thread_state_ref.state_ref = SCC->root;
                Storages[thread_state_ref.thread_id]->get_app_by_ref(thread_state_ref.state_ref,appendix);
                appendix.visited = 1;
                appendix.inset = -3;
                Storages[thread_state_ref.thread_id]->set_app_by_ref(thread_state_ref.state_ref,appendix);
                LP_queues[ii]->push(thread_state_ref);
              }
              has_job[ii].value = 1;
            }
            else
            {
              if(has_job[ii].value == 2)
              {
                SCC_id.state_ref = SCC_pointers[ii]->root;
                SCC_id.thread_id = SCC_pointers[ii]->root_id;
                SCC_list_iterator =  Complete_SCC_list.find(SCC_id);
                if(SCC_list_iterator != Complete_SCC_list.end())
                {
                  //cout<<"solved = "<<SCC_pointers[ii]->root<<endl;
                  for(thread_set_iter = SCC_list_iterator->second->predecessors->begin(); thread_set_iter != SCC_list_iterator->second->predecessors->end(); thread_set_iter++)
                  {
                    SCC_pred = Complete_SCC_list.find(*thread_set_iter)->second;
                    SCC_pred->succs--;
                    //cout<<"pred 2 = "<<thread_set_iter->state_ref<<" succs = "<<SCC_pred->succs<<endl;
                    if(SCC_pred->succs == 0 )
                    {
                      SCC_id.state_ref = SCC_pred->root;
                      SCC_id.thread_id = SCC_pred->root_id;
                      Terminal_SCC_list.push(SCC_id);
                    }
                  }
                  Complete_SCC_list.erase(SCC_list_iterator);
                }
                job_sent = true;
                if(SCC->initial)
                {
                  finnishing_id = ii;
                  finnishing_job_started = true;
                }
                SCC_pointers[ii] = SCC;
                if(SCC->size != 1)
                {
                  thread_state_ref.thread_id = SCC->root_id;
                  thread_state_ref.state_ref = SCC->root;
                  Storages[thread_state_ref.thread_id]->get_app_by_ref(thread_state_ref.state_ref,appendix);
                  appendix.visited = 1;
                  appendix.inset = -3;
                  Storages[thread_state_ref.thread_id]->set_app_by_ref(thread_state_ref.state_ref,appendix);
                  LP_queues[ii]->push(thread_state_ref);
                }
                has_job[ii].value = 1;
                }
            }
          }
        }
      }
      else
      {
        int ii;
        for(ii = 1; ii < Thread_count; ii++)
        {
          if(has_job[ii].value == 2)
          {
            SCC_id.state_ref = SCC_pointers[ii]->root;
            SCC_id.thread_id = SCC_pointers[ii]->root_id;
            SCC_list_iterator =  Complete_SCC_list.find(SCC_id);
            if(SCC_list_iterator != Complete_SCC_list.end())
            {
              //cout<<"solved = "<<SCC_pointers[ii]->root<<endl;
              for(thread_set_iter = SCC_list_iterator->second->predecessors->begin(); thread_set_iter != SCC_list_iterator->second->predecessors->end(); thread_set_iter++)
              {
                SCC_pred = Complete_SCC_list.find(*thread_set_iter)->second;
                SCC_pred->succs--;
                //cout<<"pred 3 = "<<thread_set_iter->state_ref<<" succs = "<<SCC_pred->succs<<endl;
                if(SCC_pred->succs == 0 )
                {
                  SCC_id.state_ref = SCC_pred->root;
                  SCC_id.thread_id = SCC_pred->root_id;
                  Terminal_SCC_list.push(SCC_id);
                }
              }
              Complete_SCC_list.erase(SCC_list_iterator);
            }
          }
        }
      }
   }
   else
   {
    if(has_job[id].value == 1)
    {
      SCC = SCC_pointers[id];
      //cout<<"Starting SCC size = "<<SCC->size<<" root = "<<SCC->root<<" connections = "<<SCC->connections->size()<<endl;
      thread_state_ref_t succ_thread_state_ref;

      if(SCC->size == 1)
      {
        Storages[SCC->root_id]->get_app_by_ref(SCC->root,appendix);
        if(appendix.is_seed)
        {
          appendix.value = 1;
          Storages[SCC->root_id]->set_app_by_ref(SCC->root,appendix);
          number_of_triv_equations[id]++;
          has_job[id].value = 2;
         //cout<<"seed = "<<SCC->root<<" value = "<<appendix.value<<" size of pred = "<<SCC_list_iterator->second->predecessors->size()<<endl;
        }
        else
        {
          Storages[SCC->root_id]->get_app_by_ref(SCC->root,pred_appendix);
          s = Storages[SCC->root_id]->reconstruct(SCC->root);
          //cout<<"root = "<<SCC->root<<endl;
          sys->get_succs_ordered_by_prob_and_property_trans(s,succs_cont);
          iter_i = succs_cont.begin(); 
          while(iter_i != succs_cont.end())
          {
            state_t r = (*iter_i).state;
            Storages[partition_function(r)]->is_stored(r,ref);
            Storages[partition_function(r)]->get_app_by_ref(ref,appendix);
            if ( (int)iter_i->prob_and_property_trans.prob_trans_gid == -1) // non-probabilistic
            {
              if( (appendix.inset >= info.inset || appendix.inset == -3) && (SCC->root != ref) )
              { 
                //cout<<"succ = "<<ref<<" value = "<<appendix.value<<endl;
                if(pred_appendix.value < appendix.value) pred_appendix.value = appendix.value;
                number_of_triv_equations[id]++;
              }
              delete_state(r);
              iter_i++;
            }
            else
            {
              long double right_value = 0;
              long double koef = 0;
              iter_j = iter_i;
              state_ref_t ref_i = ref;

              if(ref == SCC->root) //probabilisitc self loop
              {
                koef = koef + (((long double)iter_i->weight)/((long double)iter_i->sum));
              }
              prob_and_property_trans_t previous_action;
              previous_action = ((*iter_i).prob_and_property_trans);
              iter_i++;
              while (iter_i != succs_cont.end() && previous_action == ((*iter_i).prob_and_property_trans))
              {
                state_t r = (*iter_i).state;
                Storages[partition_function(r)]->is_stored(r,ref);
                if(ref == SCC->root) //probabilisitc self loop
                {
                  koef = koef + (((long double)iter_i->weight)/((long double)iter_i->sum));
                }
                iter_i++;
              }

              iter_i = iter_j;
              koef = 1.0 - koef;

              if( (appendix.inset >= info.inset || appendix.inset == -3) && (SCC->root != ref_i) )
              { 
                //cout<<"prob succ = "<<ref<<" value = "<<appendix.value<<endl;
                right_value = right_value + appendix.value*(((long double)iter_i->weight)/((long double)iter_i->sum)/koef);
              }
              delete_state(r);
              iter_i++;
              while (iter_i != succs_cont.end() && previous_action == ((*iter_i).prob_and_property_trans))
              {
                state_t r = (*iter_i).state;
                Storages[partition_function(r)]->is_stored(r,ref);
                Storages[partition_function(r)]->get_app_by_ref(ref,appendix);
                if( (appendix.inset >= info.inset || appendix.inset == -3) && (SCC->root != ref) )
                { 
                  //cout<<"prob succ = "<<ref<<" value = "<<appendix.value<<endl; 
                  right_value = right_value + appendix.value*(((long double)iter_i->weight)/((long double)iter_i->sum)/koef);
                }
                delete_state(r);
                iter_i++;
               }
               if(pred_appendix.value < right_value) pred_appendix.value = right_value;
               number_of_triv_equations[id]++;
             }
          }
          delete_state(s);
          if(SCC->initial)
          {
            Solved = true;
            Result =  pred_appendix.value;
          }
          Storages[SCC->root_id]->set_app_by_ref(SCC->root,pred_appendix);
          if( mode == 'd'||mode == 'a' ) cout<<SCC->root<<" Trivial component. End value = "<<pred_appendix.value<<endl;
          has_job[id].value = 2;
        }
      }
      else
      {
        lprec *lp;
        int *colno = NULL;
        REAL *row = NULL;
        colno = (int *) malloc(SCC->size * sizeof(*colno));
        row = (REAL *) malloc(SCC->size * sizeof(*row));
        int lp_index = 0;
        int variable_id = 1;
        lp = make_lp(0, SCC->size);
        set_add_rowmode(lp, TRUE);
        while(!LP_queues[id]->empty())
        {
          thread_state_ref = LP_queues[id]->front();
          LP_queues[id]->pop();
          Storages[thread_state_ref.thread_id]->get_app_by_ref(thread_state_ref.state_ref,pred_appendix);
          if(thread_state_ref.state_ref == initial_ref)
          {
            initial_visited = pred_appendix.visited;
          }
          s = Storages[thread_state_ref.thread_id]->reconstruct(thread_state_ref.state_ref);
          sys->get_succs_ordered_by_prob_and_property_trans(s,succs_cont);
          iter_i = succs_cont.begin();
          while(iter_i != succs_cont.end())
          {
            state_t r = (*iter_i).state;
            Storages[partition_function(r)]->is_stored(r,ref);
            Storages[partition_function(r)]->get_app_by_ref(ref,appendix);
            if ( (int)iter_i->prob_and_property_trans.prob_trans_gid == -1) // non-probabilistic
            {
              if(appendix.inset >= info.inset || appendix.inset == -3) // in this SCC generate equations
              {
                if(appendix.root == pred_appendix.root)
                {
                  if(appendix.inset != -3)
                  {
                    appendix.inset = -3;
                    succ_thread_state_ref.thread_id = partition_function(r);
                    succ_thread_state_ref.state_ref = ref;
                    LP_queues[id]->push(succ_thread_state_ref);
                    variable_id++;
                    appendix.visited = variable_id;
                    Storages[partition_function(r)]->set_app_by_ref(ref,appendix);
                  }
                  if(appendix.visited != pred_appendix.visited)
                  {
                    lp_index = 0;
                    colno[lp_index] = pred_appendix.visited; /* first column */
                    row[lp_index++] = 1;
                    colno[lp_index] = appendix.visited; /* second column */
                    row[lp_index++] = -1;
                    add_constraintex(lp, lp_index, row, colno, GE, 0);
                    number_of_equations[id]++;
                  }
                }
                else // not in this SCC generate equations
                {
                  lp_index = 0;
                  colno[lp_index] = pred_appendix.visited; /* first column */
                  row[lp_index++] = 1;
                  add_constraintex(lp, lp_index, row, colno, GE, appendix.value);
                  number_of_equations[id]++;
                }
              }
              delete_state(r);
              iter_i++;
            }
            else // probabilistic
            {
              long double right_value = 0;
              lp_index = 0;
              colno[lp_index] = pred_appendix.visited; /* first column */
              row[lp_index++] = 1;
              if(appendix.inset >= info.inset || appendix.inset == -3) // in this SCC generate equations
              {
                if(appendix.root == pred_appendix.root)
                {
                  if(appendix.inset != -3)
                  {
                    appendix.inset = -3;
                    succ_thread_state_ref.thread_id = partition_function(r);
                    succ_thread_state_ref.state_ref = ref;
                    LP_queues[id]->push(succ_thread_state_ref);
                    variable_id++;
                    appendix.visited = variable_id;
                    Storages[partition_function(r)]->set_app_by_ref(ref,appendix);
                  }
                  colno[lp_index] = appendix.visited; /* second column */
                  row[lp_index++] = -1*(((long double)iter_i->weight)/((long double)iter_i->sum));
                }
                else // not in this SCC generate equations
                {
                  right_value = right_value + appendix.value*(((long double)iter_i->weight)/((long double)iter_i->sum));
                }
              }
              delete_state(r);
              prob_and_property_trans_t previous_action;
              previous_action = ((*iter_i).prob_and_property_trans);
              iter_i++;
              while (iter_i != succs_cont.end() && previous_action == ((*iter_i).prob_and_property_trans))
              {
                state_t r = (*iter_i).state;
                Storages[partition_function(r)]->is_stored(r,ref);
                Storages[partition_function(r)]->get_app_by_ref(ref,appendix);
                if(appendix.inset >= info.inset || appendix.inset == -3) // in this SCC generate equations
                {
                  if(appendix.root == pred_appendix.root)
                  {
                    if(appendix.inset != -3)
                    {
                      appendix.inset = -3;
                      succ_thread_state_ref.thread_id = partition_function(r);
                      succ_thread_state_ref.state_ref = ref;
                      LP_queues[id]->push(succ_thread_state_ref);
                      variable_id++;
                      appendix.visited = variable_id;
                      Storages[partition_function(r)]->set_app_by_ref(ref,appendix);
                    }
                    colno[lp_index] = appendix.visited; /* second column */
                    row[lp_index++] = -1*(((long double)iter_i->weight)/((long double)iter_i->sum));
                  }
                  else // not in this SCC generate equations
                  {
                    right_value = right_value + appendix.value*(((long double)iter_i->weight)/((long double)iter_i->sum));
                  }
                }
                delete_state(r);
                iter_i++;
              }
              add_constraintex(lp, lp_index, row, colno, GE, right_value);
              number_of_equations[id]++;
            }
          }
          delete_state(s);
        }
        set_add_rowmode(lp, FALSE);
        if(SCC->initial) 
        {
          lp_index = 0;
          colno[lp_index] = initial_visited; 
          row[lp_index++] = 1;
        }
        else
        {
          for(thread_set_iter = SCC->connections->begin(); thread_set_iter != SCC->connections->end(); thread_set_iter++)
          {
            Storages[thread_set_iter->thread_id]->get_app_by_ref(thread_set_iter->state_ref,appendix);
            lp_index = 0;
            colno[lp_index] = appendix.visited; 
            row[lp_index++] = 1;
          }
        }
        set_obj_fnex(lp, lp_index, row, colno);
        set_minim(lp);
        set_simplextype(lp, SIMPLEX_DUAL_DUAL);
        //write_LP(lp, stdout);
        set_verbose(lp, CRITICAL);
        if( mode == 'd'||mode == 'a' ) cout<<"Solving SCC of size = "<<SCC->size<<" ...."<<endl;
        solve(lp);
        if( mode == 'd'||mode == 'a' ) cout<<"SCC of size = "<<SCC->size<<" Solved"<<endl;
        get_variables(lp, row);
        //for(lp_index = 0; lp_index < SCC->size; lp_index++)
        //cout<<"Variable "<<lp_index + 1<<" =  "<<row[lp_index]<<endl;
        if(SCC->initial)
        {
          Result = row[initial_visited - 1 ]; 
        }
        else
        {
          for(thread_set_iter = SCC->connections->begin(); thread_set_iter != SCC->connections->end(); thread_set_iter++)
          {
            Storages[thread_set_iter->thread_id]->get_app_by_ref(thread_set_iter->state_ref,appendix);
            appendix.value = row[appendix.visited - 1 ];
            if( mode == 'd'||mode == 'a' ) cout<<thread_set_iter->state_ref<<"Nontrivial component End value = "<<appendix.value<<endl;
            Storages[thread_set_iter->thread_id]->set_app_by_ref(thread_set_iter->state_ref,appendix);
          }
        }
        if(row != NULL) free(row);
        if(colno != NULL) free(colno);
        if(lp != NULL) delete_lp(lp);
        has_job[id].value = 2;
      }
    }
   }
  }

 }

 if( id == 0 && ( mode == 'd' || mode == 'a') )
 {
   cout <<"------------------------------------------------------------------------"<<endl;
   cout <<"The result of the system = "<<Result<<endl;
   cout <<"------------------------------------------------------------------------"<<endl;

 }

 if(id == 0)
 {
   for(int i=0;i < Thread_count; i++)
   {
     Number_of_equations = Number_of_equations + number_of_equations[i];
     Number_of_triv_equations = Number_of_triv_equations + number_of_triv_equations[i];
     
   }
 }
 if( id == 0 )
 {
   if (mode!='q')
   {
     cout <<"Done."<<endl;
     cout <<"-------------------------------------------------------------------------"<<endl;
     cout <<"number of variables:         "<<tmp_explored_state<<endl;
     cout <<"number of equations:         "<<Number_of_equations<<endl;
     cout <<"number of trivial equations: "<<Number_of_triv_equations<<endl;
     cout <<"number of all equations:     "<<Number_of_triv_equations + Number_of_equations<<endl;
     cout <<"time                         "<<timer.gettime()<<" s"<<endl;
     cout <<"LP time                      "<<lp_timer.gettime()<<" s"<<endl;
     cout <<"-------------------------------------------------------------------------"<<endl;
   }
   cout <<"========================================================================="<<endl;
   cout <<"LTL formula is satisfied with minimal probability "<<setprecision(15);
   cout <<1.0 - Result<<setprecision(6)<<endl;

   cout <<"========================================================================="<<endl;
 }
}
 delete sys; 
 pthread_exit(NULL);
}


void version()
{
  cout <<"----------------------------------------------------------------------------"<<endl;
  cout <<"  ProbDiVinE-MC: A Parallel Qualitative and Quantitative LTL Model Checker  "<<endl;
  cout <<"  " << divine::versionString() << " (" << divine::buildDateString() << ")" << endl;
  cout <<"----------------------------------------------------------------------------"<<endl;
}

void usage()
{
  version();
  cout <<"+--------------------------------------------------------------------------+"<<endl;
  cout <<"Usage: probdivine-mc [options] input_file"<<endl;
  cout <<"Options: "<<endl;
  cout <<" -v, show version"<<endl;
  cout <<" -h, show this help"<<endl;
  //cout <<" -d, debugging mode"<<endl;
  //cout <<" -a, show all results and statistics"<<endl;
  cout <<" -b, show basic statistics (default)" <<endl;
  cout <<" -q, quite mode"<<endl;
  cout <<" -t, quantitative verification (default)"<<endl;
  cout <<" -l, qualitative verification only"<<endl;
  cout <<" -n X, set number of threads to X (default X = 1)"<<endl; 
  cout <<"+--------------------------------------------------------------------------+"<<endl;
}


int main(int argc, char** argv) 
{ 

  // default setting
  Thread_count = 1;
  mode = 'b';
  type = 't';

  int c;

  while ((c = getopt (argc, argv, "dabqtlvhn:")) != -1)
  {
    switch (c)
    {
      case 'h': 
        usage();
        return 0;
        break;
      case 'v' :
        version();
        return 0;
        break;
      case 'd':
       mode = 'd';
       break;
      case 'a':
       mode = 'a';
       break;
     case 'b':
       mode = 'b';
       break;
     case 'q':
       mode = 'q';
       break;
     case 't':
       type = 't';
       break;
     case 'l':
       type = 'l';
       break;
     case 'n':
       Thread_count = atoi(optarg);
       if ( Thread_count <= 0 )
       {
        cout<<"Invalid number of working threads "<<endl;
        usage();
        return 0;
       } 
       break;
     case '?':
       if (optopt == 'n')
        cout<<"Option 'n' requires an argument."<<endl;
       else 
        cout<<"Unknown option."<<endl; 
       usage();
       return 0;
   }
 }

  if (argc < optind+1)
    {
      usage();
      return 0;
    }

  if(mode != 'q')
  {
    version();
    cout << "Number of threads:           "<< Thread_count <<endl;
    if (type == 'l') cout <<"Verification:                qualitative"<<endl;
    if (type == 't') cout <<"Verification:                quantitative"<<endl;
    if (mode == 'd') cout <<"Mode:                        debugging"<<endl;
    if (mode == 'a') cout <<"Mode:                        all statistics and results"<<endl;
    if (mode == 'b') cout <<"Mode:                        basic statistics"<<endl;
    if (mode == 'q') cout <<"Mode:                        quiet"<<endl;
    cout <<"========================================================================="<<endl;
  }
  pthread_t thread_id[Thread_count];

  //Info variables inicialization
  states.resize(Thread_count);
  transitions.resize(Thread_count);
  number_of_trivial_SCC.resize(Thread_count);
  number_of_equations.resize(Thread_count);
  number_of_triv_equations.resize(Thread_count);

  //Buffers inicializations
  Buffers.resize(Thread_count);
  for(int i=0; i < Thread_count; i++) 
   Buffers[i].resize(Thread_count);

   //SCC Buffers inicializations
   Buffers_SCC.resize(Thread_count);

   //Queues inicialization
  Queues.resize(Thread_count);

  //LP_queues inicialization
  LP_queues.resize(Thread_count);


  //Queues_init inicialization
  Queues_init.resize(Thread_count);
  for(int i=0; i < Thread_count; i++) 
   Queues_init[i].resize(6);
  for(int i=0; i < Thread_count; i++)
   Queues_init[i][0] = new queue<state_ref_t>;

  //accepting queues inicializations
  accepting_queues.resize(Thread_count);

  //Storages inicialization
  Storages.resize(Thread_count);
  for(int i=0; i < Thread_count; i++)
   Storages[i] = new explicit_storage_t;

   //Job_infos_init inicialization
   Job_infos_init.resize(Thread_count);
   for(int i=0; i < Thread_count; i++) 
    Job_infos_init[i].resize(6);

   //Job_infos inicialization
   Job_infos.resize(Thread_count);

    //SCC pointers inicialization
    SCC_pointers.resize(Thread_count);

    //has_job inicialization
    has_job.resize(Thread_count);

   //Mutex inicialization
   pthread_mutex_init( &mutex, NULL);

   //waiting_status inicialization
   waiting_status.resize(Thread_count);
   for(int i=0; i < Thread_count; i++)
    waiting_status[i].value = false;

   //synchronization inicialization
   synchronization_alfaro.resize(Thread_count);
   synchronization_scc.resize(Thread_count);
   synchronization_scc_graph.resize(Thread_count);
   inicialization.resize(Thread_count);
   for(int i=0; i < Thread_count; i++)
   {
      synchronization_alfaro[i].value = false;
      synchronization_scc[i].value = false;
      synchronization_scc_graph[i].value = false;
      inicialization[i].value = false;
   }

   //creating_status inicialization
   creating_status.resize(Thread_count);
   for(int i=0; i < Thread_count; i++)
    creating_status[i].value = 0;

   //SCC_lists inicialization
   SCC_lists.resize(Thread_count);

  // Thread inicialization
  dve_prob_explicit_system_t* sys;
  inicialization_info.resize(Thread_count);
  for(int i=0; i < Thread_count; i++)
  {
     if(mode != 'q' && i==0) cout <<"Reading probdve source.............................................." ;
     sys = new dve_prob_explicit_system_t(gerr);
     if (sys->read(argv[optind]))
    {
     return 1;
    }
    else
    {
      if(mode != 'q'&& i==0) 
      {
         cout << "Done." <<endl;
         cout <<"-------------------------------------------------------------------------"<<endl;
      }
    }
    inicialization_info[i].sys = sys;
    inicialization_info[i].id = i;
  }

  // Thread creation
  for(int i=0; i < Thread_count; i++)
  {
   pthread_create( &thread_id[i], NULL, thread_function, (void *) &inicialization_info[i] );
  }

  // Thread synchronization
  for(int i=0; i < Thread_count; i++)
   pthread_join( thread_id[i], NULL);

  for(int i=0; i < Thread_count; i++)
   delete(Storages[i]);

  cout <<"Model statistic:"<<endl;
  cout <<"states:                      "<<States<<endl;
  cout <<"transitions:                 "<<Transitions<<endl;
  cout <<"all memory                   "<<vm.getvmsize()/1024.0<<" MB"<<endl;
  cout <<"time:                        "<<timer.gettime()<<" s"<<endl;
  cout <<"========================================================================="<<endl;

  return 0;
}

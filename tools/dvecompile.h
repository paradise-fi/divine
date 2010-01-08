// -*- C++ -*- (c) 2009, 2010 Milan Ceska & Petr Rockai
#include <divine/legacy/system/dve/dve_explicit_system.hh>
#include <string>
#include <math.h>
#include <map>
#include <vector>

using namespace divine;
using namespace std;

struct ext_transition_t
{
    int synchronized;
    dve_transition_t *first;
    dve_transition_t *second; //only when first transition is synchronized;
    dve_transition_t *property; // transition of property automaton
};

struct dve_compiler: public dve_explicit_system_t
{
    int current_size;
    map<size_int_t,map<size_int_t,vector<ext_transition_t> > > transition_map;
    map<size_int_t,vector<dve_transition_t*> > channel_map;

    vector<dve_transition_t*> property_transitions;
    vector<dve_transition_t*>::iterator iter_property_transitions;

    map<size_int_t,vector<dve_transition_t*> >::iterator iter_channel_map;
    vector<dve_transition_t*>::iterator iter_transition_vector;
    map<size_int_t,vector<ext_transition_t> >::iterator iter_process_transition_map;
    map<size_int_t,map<size_int_t,vector<ext_transition_t> > >::iterator iter_transition_map;
    vector<ext_transition_t>::iterator iter_ext_transition_vector;

    dve_compiler(error_vector_t & evect=gerr)
        : explicit_system_t(evect), dve_explicit_system_t(evect), current_size(0)
    {}
    virtual ~dve_compiler() {}

    void write_C(dve_expression_t & expr, std::ostream & ostr, std::string state_name);
    void print_DVE_compiler(std::ostream & ostr);
    void print_test_generator(std::ostream & ostr);
    void print_DiVinE2_generator(ostream & ostr);
    void print_state_struct(std::ostream & ostr);
    void print_state_t(std::ostream & ostr);
    void print_duplicate_state(std::ostream & ostr);
    void print_new_state(std::ostream & ostr);
    void print_include(std::ostream & ostr);
    void print_test_header(std::ostream & ostr);
    void print_main(std::ostream & ostr);
    void print_initial_state_struct(std::ostream & ostr);
};

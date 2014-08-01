#ifndef COIN_SEQ_TRANSITION_H
#define COIN_SEQ_TRANSITION_H

#include <divine/coin/parse/coin_common.hh>

namespace coin_seq {

struct transition_t {

  	transition_t (int aut, int ts, const label_t &l)
	  	: label(l), automaton(aut), to_state(ts) {}

        virtual ~transition_t() {};
	
	bool is_simple() const { return !is_sync(); }
	virtual bool is_sync() const { return false; }
	virtual bool has_prop() const { return false; }

	int get_automaton() const { return automaton; }
	int get_to_state() const { return to_state; }
	const label_t & get_label() const { return label; }
	virtual int get_automaton2() const { return -1; }
	virtual int get_to_state2() const { return -1; }

	virtual int get_prop_to_state() const { return -1; }

	label_t label;

	int automaton;
	int to_state;

	virtual void get_effect(const vector<int> & state_vector,
                                vector<int> & next_state_vector) {
	  next_state_vector = state_vector;
	  next_state_vector[automaton] = to_state;
	}

};


struct sync_transition_t : public transition_t {

	sync_transition_t(int aut, int ts, int aut2, int ts2, const label_t & l)
	  : transition_t(aut, ts, l), automaton2(aut2), to_state2(ts2) {};

        virtual ~sync_transition_t() {};

  	//  automaton is the SENDING automaton	(output label)
  	// automaton2 is the RECEIVING automaton (input label)
	int automaton2;
	int to_state2;
  	
	bool is_sync() const { return true; }

	int get_automaton2() const { return automaton2; }
	int get_to_state2() const { return to_state2; }
	
	virtual void get_effect(const vector<int> & state_vector,
                                vector<int> & next_state_vector) {
	  next_state_vector = state_vector;
	  next_state_vector[automaton] = to_state;
	  next_state_vector[automaton2] = to_state2;
	}
};

struct transition_with_property_t : public transition_t {
	
	transition_with_property_t(int aut, int ts, const label_t & label, int pts)
	  : transition_t(aut,ts,label), prop_to_state(pts) {}

	transition_with_property_t(const transition_t & t, int pts)
	  : transition_t(t), prop_to_state(pts) {}

        virtual ~transition_with_property_t() {}

	int prop_to_state;

	virtual void get_effect(const vector<int> & state_vector,
	                              vector<int> & next_state_vector) {
	 	  next_state_vector = state_vector;
	  	  next_state_vector[automaton] = to_state;
		  next_state_vector[state_vector.size() - 1] = prop_to_state;
	}

};


struct sync_transition_with_property_t : public sync_transition_t {
	
	sync_transition_with_property_t(int aut, int ts, int aut2, int ts2,
	    	const label_t & label, int pts)
	  : sync_transition_t(aut, ts, aut2, ts2, label), prop_to_state(pts) {}

	sync_transition_with_property_t(const sync_transition_t & t, int pts)
	  : sync_transition_t(t), prop_to_state(pts) {}
	
        virtual ~sync_transition_with_property_t() {};

	int prop_to_state;

        virtual void get_effect(const vector<int> & state_vector,
	                        vector<int> & next_state_vector) {
		next_state_vector = state_vector;
		next_state_vector[automaton] = to_state;
		next_state_vector[automaton2] = to_state2;
		next_state_vector[state_vector.size() - 1] = prop_to_state;
	}
};

}
#endif

/*
 * transition.cc
 *
 *  Created on: 30.10.2010
 *      Author: milan
 */

#include "transition.h"

namespace coin_seq {

bool compare_transitions(const transition_t & t1, const transition_t &t2) {
  // -1 means t1 < t2
  // +1 means t1 > t2
  //  0 means t1 == t2
	if (!t1.is_sync() && t2.is_sync()) return -1;
	if (t1.is_sync() && !t2.is_sync()) return 1; // simple < sync

	if (t1.get_label() < t2.get_label()) return -1;
	if (t1.get_label() > t2.get_label()) return +1; // labels

	if (t1.get_automaton() < t2.get_automaton()) return -1;
	if (t1.get_automaton() > t2.get_automaton()) return +1; // aut

	if (t1.get_to_state() < t2.get_to_state()) return -1;
	if (t1.get_to_state() > t2.get_to_state()) return +1; // to_state

	if (t1.is_sync()) {
		if (t1.get_automaton2() < t2.get_automaton2()) return -1;
		if (t1.get_automaton2() > t2.get_automaton2()) return +1;
		if (t1.get_to_state2() < t2.get_to_state2()) return -1;
		if (t1.get_to_state2() > t2.get_to_state2()) return +1;
	}

	// they are identical
	return 0;
}

bool operator==(const transition_t & t1, const transition_t &t2) {
  	return compare_transitions(t1, t2) == 0;
}

bool operator<(const transition_t & t1, const transition_t & t2) {
  	return compare_transitions(t1, t2) == -1;
}

bool operator>(const transition_t & t1, const transition_t & t2) {
  	return compare_transitions(t1, t2) == 1;
}

}  // namespace coin_seq

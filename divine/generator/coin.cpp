/*
 * coingenerator.cc
 *
 *  Created on: 30.10.2010
 *      Author: Milan Krivanek
 */

#if GEN_COIN
#include <divine/generator/coin.h>

namespace divine {
namespace generator {

int Coin::count = 0;

Coin::Coin() :
    por(false), coin_system(NULL) {
    id = count++;
#ifdef VERBOSE
    std::cout << id << ": Empty constructor" << std::endl;
#endif
#ifdef LCA
    lca = NULL;
    restrict = NULL;
    only = NULL;
    empty_label_set = NULL;
#endif
}

Coin::Coin(const Coin & other) :
    Common(other),
    file(other.file),
    por(other.por),
    coin_system(NULL),
    original_slack(other.original_slack) {
    id = count++;
#ifdef VERBOSE
    std::cout << "Creating instance " << id << " as a copy of " << other.id << std::endl;
#endif
#ifdef LCA
    lca = NULL;
    restrict = NULL;
    only = NULL;
    empty_label_set = NULL;
#endif
}

Coin::~Coin() {
#ifdef VERBOSE
    std::cout << "Deleting instance " << id << " initialized with " << file << std::endl;
#endif
    if (coin_system) {
        delete coin_system;
    }
    for (unsigned i = 0; i < tree_nodes.size(); i++) {
        delete tree_nodes[i];
    }
    tree_nodes.clear();

#ifdef LCA
    if(lca != NULL) {
        for (int i = 0; i < system_primitive_components.size(); i++) {
            free(lca[i]);
        }
        free(lca);
        lca = NULL;
    }
    if(restrict != NULL && only != NULL) {
        for(int i = 0; i < automata_size; i++) {
            for(int j = 0; j <= i; j++) {
                if(restrict[i][j] != NULL && restrict[i][j] != empty_label_set) {
                    delete restrict[i][j];
                }
                if(only[i][j] != NULL && only[i][j] != empty_label_set) {
                    delete only[i][j];
                }
            }
            free(restrict[i]);
            free(only[i]);
        }
        free(restrict);
        free(only);
    }
    if(empty_label_set != NULL) {
        delete empty_label_set;
    }
#endif
}

coin_system_t * Coin::getCoinSystem() {
    if (!coin_system) {
        coin_system = new coin_system_t;
        if (!file.empty()) {
#ifdef VERBOSE
            std::cout << id << ": Parse" << std::endl;
#endif
            the_coin_system = coin_system;
            yyin = fopen(file.c_str(), "r");
            if (!yyin) {
                throw std::runtime_error("Error reading input model.");
            }

            yyparse(); // build the coin system
            fclose(yyin);
            init();
        }
    }
    return coin_system;
}

void Coin::initPOR() {
#ifdef VERBOSE
    std::cout << id << ": Init POR" << std::endl;
#endif
    por = true;
}

Coin::Node Coin::_initial() {
    getCoinSystem();

    Blob b = newNode();
    packPrimitiveStates(primitive_states, state_vector);
    State s(primitive_states);
    s.pack(pool(), b, getTotalSlack());

    return b;
}

void Coin::read(std::string path, std::vector< std::string >, Coin *) {
#ifdef VERBOSE
    std::cout << id << ": Read '" << path << "'" << std::endl;
#endif
    file = path;
    getCoinSystem();
}

Coin & Coin::operator=(const Coin &other) {
#ifdef VERBOSE
    std::cout << "Assignment: " << id << " := " << other.id << std::endl;
#endif
    Common::operator=(other);
    file = other.file;
    original_slack = other.original_slack;
    if (coin_system != NULL) {
        delete coin_system;
        coin_system = NULL;
    }
    getCoinSystem(); // FIXME, we force read here to keep parsing from happening in
    // multiple threads at once, no matter the mutex...
    return *this;
}

int Coin::setSlack(int s) {
#ifdef VERBOSE
    std::cout << id << ": setSlack() " << s << std::endl;
#endif

    original_slack = s;
    return Common::setSlack(por ? s + sizeof(Extension) : s);
}

inline int Coin::getTotalSlack() {
    return this->slack();;
}

inline size_t Coin::getSize(unsigned int number_of_states) {
    if (number_of_states < 256) {
        return 1;
    } else if (number_of_states < 65536) {
        return 2;
    } else if (number_of_states < 16777216) {
        return 3;
    } else {
        return 4;
    }
}

Coin::Successors Coin::_successors(Node compressed_state) {
    vector<transition_t *> * enabled_trans = getEnabledTrans(compressed_state);

    return apply(compressed_state, enabled_trans);
}

Coin::Successors Coin::_ample(Node compressed_state) {
    assert(por);

    vector<transition_t *> * enabled_trans = getEnabledTrans(compressed_state);

    int i = getAmpleSet(compressed_state);
    if (i < 0) {
        i = findAmpleSet(enabled_trans); //try to find an ample set
        //std::cout << "Found ample set ID: " << i << std::endl;
        if (i == -1) { // not found
            i = system_aut_id; // fully expand
        }
        setAmpleSet(compressed_state, i); // store ample set ID
    } else {
        //std::cout << "Stored ample set ID: " << i << std::endl;
    }

    assert(i >= 0);

    if (i != system_aut_id) {
        //if non-empty ample set that is a proper subset of system automata was found
        vector<transition_t *>::iterator it;

        //resetting of the in_composition flags for all automata, then setting them for I
        setCandidateFlags(i);

        //filtering enabled transitions that do not belong to automata in I
        vector<transition_t *> * ample_trans = new vector<transition_t *> ;
        for (it = enabled_trans->begin(); it != enabled_trans->end(); it++) {
            transition_t * tr = *it;
            if (tr->is_sync()) {
                if ((!tree_nodes[tr->get_automaton()]->in_composition)
                        && (!tree_nodes[tr->get_automaton2()]->in_composition)) {
                    delete tr;
                    continue;
                }
            } else {
                if (!tree_nodes[tr->get_automaton()]->in_composition) {
                    delete tr;
                    continue;
                }
            }
            ample_trans->push_back(tr);
        }
        delete enabled_trans;
        return apply(compressed_state, ample_trans);
    } else {
        return apply(compressed_state, enabled_trans);
    }
}

bool Coin::isAccepting(Node compressed_state) {
    if (!property) {
        return false;
    }
    // ID of the current state of the property automaton = the last element of the vector
    unsigned int sid = State::getPrimitiveState(pool(), compressed_state, getTotalSlack(), packed_state_size-1);
    return (property->accepting_states.find(sid) != property->accepting_states.end());
}

std::string Coin::showNode(Node compressed_state) {
    std::stringstream stream;
    State state(pool(), compressed_state, getTotalSlack());
    vector<int> v = state.getValues(); //primitive
    stream << '[';
    int n = 0;
    for(int i = 0; i < automata_size; i++) {
        aut_t * tmpa = automata->get(i);
        if(tmpa->is_prim()) {
            if(n > 0) {
                stream << ", ";
            }
            int sid = v[n];
            string name = tmpa->states[sid]->name;
            stream << name;
            n++;
        }
    }
    if(property) {
        if(n > 0) {
            stream << ", ";
        }
        int sid = v[n];
        string name = property->prop_states[sid]->name;
        stream << name;
        n++;
    }
    stream << ']';
    return stream.str();
}

std::string Coin::showTransition( Node, Node, Label )
{
    return "";
}

void Coin::packPrimitiveStates(vector<int> & packed_state, vector<int> & unpacked_state) {
    int j = 0;
    for(int i = 0; i < automata_size; i++) {
        if(automata->get(i)->is_primitive) {
            packed_state[j] = unpacked_state[i];
            j++;
        }
    }

    if(property) {
        packed_state[j] = unpacked_state[prop_aut_id];
    }
}

void Coin::unpackPrimitiveStates(vector<int> & packed_state, vector<int> & unpacked_state) {
    int j = 0;
    for(int i = 0; i < automata_size; i++) {
        if(automata->get(i)->is_primitive) {
            unpacked_state[i] = packed_state[j];
            j++;
        }
    }

    if(property) {
        unpacked_state[prop_aut_id] = packed_state[j];
    }
}

void Coin::init() {
#ifdef VERBOSE
    std::cout << id << ": Init, original slack: " << original_slack << std::endl;
#endif

    assert(coin_system);

    automata = coin_system->get_automata_list();
    property = coin_system->property;
    system_aut_id = coin_system->system_aut_id;
    automata_size = automata->size();
    packed_state_size = 0;
    state_size = automata_size + (property ? 1 : 0);
    prop_aut_id = property ? state_size - 1 : -1;
    state_vector.resize(state_size);
    next_state_vector.resize(state_size);
    vector<size_t> metrics;
    for (int i = 0; i < automata_size; i++) {
        aut_t * tmp = automata->get(i);
        if (tmp->is_prim()) {
            state_vector[i] = tmp->init_state;
            metrics.push_back(getSize(tmp->states.size()));
            packed_state_size++;
        } else {
            // previously -1, but we work with unsigned int;
            // 255 for easier debugging
            state_vector[i] = 255;
        }
    }
    if (property) {
        state_vector[prop_aut_id] = property->init_state;
        metrics.push_back(getSize(property->prop_states.size()));
        packed_state_size++;
    }

    metrics.resize(packed_state_size);
    primitive_states.resize(packed_state_size);

    // fixed size state metrics - 1 byte per index
    State::metrics = StateMetrics(packed_state_size, metrics);

#ifdef LCA
    initSystemPrimitiveComponents();
#ifdef VERBOSE
    std::cout << id << ": Initialized sys. prim. comp. " << std::endl;
#endif

    initLCA();
#ifdef VERBOSE
    std::cout << id << ": Initialized LCAs." << std::endl;
#endif

    initRestrictSets();
#ifdef VERBOSE
    std::cout << id << ": Initialized restrict sets." << std::endl;
#endif
#endif

    if (por) {
        coin_system->precompute();
        for(unsigned i = 0; i < tree_nodes.size(); i++) {
            delete tree_nodes[i];
        }
        tree_nodes.clear();
        for(int i = 0; i < automata_size; i++) {
            tree_nodes.push_back(new tree_node_t());
        }
    }
}

vector<sometrans> Coin::getSuccTransitions(int aid) {
    vector<sometrans> result;
    aut_t * tmpa = automata->get(aid);
    if (!tmpa->is_primitive) { // composite
        vector<sometrans> input;
        vector<sometrans> output;
        for (vector<int>::iterator i = tmpa->composed_of.begin();
                i != tmpa->composed_of.end(); i++) {
            vector<sometrans> tmp_succs = getSuccTransitions(*i);
            for (vector<sometrans>::iterator iter = tmp_succs.begin();
                    iter != tmp_succs.end(); iter++) {
                bool inr = (
                        tmpa->restrictions.find(iter->label) == tmpa->restrictions.end()
                );
                // inr == not found in restriction list
                // if not found and list is restrictL
                // or if found and list is onlyL
                // then add
                if (tmpa->is_restrict == inr) {
                    result.push_back(*iter);
                }

                if (iter->label.isInput()) {
                    input.push_back(*iter);
                } else if (iter->label.isOutput()) {
                    output.push_back(*iter);
                }
            }
        }
        for (vector<sometrans>::iterator inp = input.begin();
                inp != input.end(); inp++) {
            int inp_act_id = inp->label.act_id;
            for (vector<sometrans>::iterator outp = output.begin();
                    outp != output.end(); outp++) {
                if (inp_act_id == outp->label.act_id // same action
                        //the topmost automata the transitions belong to are different
                        && inp->top_aut_id != outp->top_aut_id) {
                    label_t newlabel;
                    newlabel.fromc_id = outp->label.fromc_id;
                    newlabel.toc_id = inp->label.toc_id;
                    newlabel.act_id = inp_act_id;

                    bool inr = (
                            tmpa->restrictions.find(newlabel) == tmpa->restrictions.end()
                    );

                    if (inr == tmpa->is_restrict) {
                        sometrans newtrans;
                        newtrans.prop = false;
                        newtrans.single = false;
                        newtrans.aut_id = inp->aut_id;
                        newtrans.state_id = inp->state_id;
                        newtrans.label = newlabel;
                        newtrans.aut_id2 = outp->aut_id;
                        newtrans.state_id2 = outp->state_id;

                        result.push_back(newtrans);
                    }
                }
            }
        }
    } else { // primitive
        int sid = state_vector[aid];
        coin_state_t * stmp = tmpa->states.get(sid);
        for (vector<trans_t *>::iterator i = stmp->outtrans.begin();
                i != stmp->outtrans.end(); i++) {
            sometrans strn;
            strn.prop = false;
            strn.single = true;
            strn.aut_id = aid;
            //      strn.trans = *i;
            strn.state_id = (*i)->tostate_id;
            strn.label = (*i)->label;
            result.push_back(strn);
        }
    }

    //set the topmost automaton the transitions belong to
    //to the current aid; do it here not to affect previous computation
    for (vector<sometrans>::iterator i = result.begin(); i != result.end(); i++) {
        i->top_aut_id = aid;
    }

    return result;
}

vector<transition_t *> * Coin::combineWithProp(vector<transition_t *> * system_trans) {

    vector<transition_t *> * trans = new vector<transition_t *> ;
    set<label_t> enabled_labels;

    for (vector<transition_t *>::iterator j = system_trans->begin();
            j != system_trans->end(); j++) {
        enabled_labels.insert((*j)->get_label());
    }

    prop_state * ps = property->prop_states.get(
            state_vector[state_size - 1]);

    for (vector<prop_trans_t*>::iterator i = ps->outtrans.begin();
            i != ps->outtrans.end(); i++) {
        prop_trans_t * pt = *i;
        if (!common_elem(enabled_labels, pt->guard_false)
                && subset(pt->guard_true, enabled_labels)) {
            // all guards are met
            for (vector<transition_t *>::iterator j = system_trans->begin();
                    j != system_trans->end(); j++) {
                transition_t * st = *j;
                if (pt->any_label != // any_label XOR j_label in labels
                        (pt->labels.find(st->get_label())
                                != pt->labels.end())) {
                    transition_t * at;

                    if (st->is_sync()) {
                        at = new sync_transition_with_property_t(
                                *(dynamic_cast<sync_transition_t *> (st)),
                                pt->tostate_id);
                    } else {
                        at = new transition_with_property_t(*st,
                                pt->tostate_id);
                    }
                    trans->push_back(at);
                }
            }
        }
    }
    return trans;
}

vector<transition_t *> * Coin::getEnabledTrans(Node compressed_state) {
    State state(pool(), compressed_state, getTotalSlack());
    primitive_states = state.getValues();
    unpackPrimitiveStates(primitive_states, state_vector);

    vector<transition_t *> * enabled_trans = new vector<transition_t *>;

    //compute successors->
    vector<sometrans> transitions;
#ifdef LCA
    transitions = getSuccTransitionsLCA();
#else
    transitions = getSuccTransitions(system_aut_id);
#endif

    vector<sometrans>::iterator i;

    for (i = transitions.begin(); i != transitions.end(); i++) {
        transition_t * t;
        if (i->single) {
            t = new transition_t(i->aut_id, i->state_id, i->label);
        } else {
            t = new sync_transition_t(i->aut_id, i->state_id,
                    i->aut_id2, i->state_id2, i->label);
        }
        enabled_trans->push_back(t);
    }


    return enabled_trans;
}

Coin::Successors Coin::apply(const Node &st, vector<transition_t *> * succ_trans) {

    // if property, combine enabled transitions with property
    if (property) {
        vector<transition_t *> * combined_trans;
        combined_trans = combineWithProp(succ_trans);
        // leak corrected
        for (vector<transition_t *>::iterator i = succ_trans->begin();
                i != succ_trans->end(); i++) {
            delete *i;
        }
        //
        delete succ_trans;
        succ_trans = combined_trans;
    }

    // we can now compute the effect of effective transitions
    std::list<Node> succs_list;
    for (vector<transition_t *>::iterator i = succ_trans->begin();
            i != succ_trans->end(); i++) {
        transition_t * t = *i;
        t->get_effect(state_vector, next_state_vector);

        Blob b = newNode();
        packPrimitiveStates(primitive_states, next_state_vector);
        State s(primitive_states);
        s.pack(pool(), b, getTotalSlack());
        succs_list.push_back(b);

        delete t;
    }
    delete succ_trans;

    Successors s(st, succs_list);
    return s;
}

void Coin::setInComposition(int i) {
    aut_t * automaton = (*automata)[i];
    tree_node_t * node = tree_nodes[i];
    node->in_composition = true;
    for (vector<int>::iterator j = automaton->composed_of.begin();
            j != automaton->composed_of.end(); j++) {
        setInComposition(*j);
    }
}

inline void Coin::setCandidateFlags(int aid) {
    // reset the flags
    for (int j = 0; j < automata_size; j++) {
        tree_nodes[j]->in_composition = false;
    }

    setInComposition(aid);
}

bool Coin::checkC1(int aid) {

    // reset the flags and set them for the subtree of aid
    setCandidateFlags(aid);
    // now: ample set candidates are in_composition

    bit_set_t * candidate_input = new bit_set_t(coin_system->act_count);
    bit_set_t * candidate_output = new bit_set_t(coin_system->act_count);
    bit_set_t * other_input = new bit_set_t(coin_system->act_count);
    bit_set_t * other_output = new bit_set_t(coin_system->act_count);

    // step 1: gather candidate actions

    for (int j = 0; j < automata_size; j++) {
        aut_t * aut = automata->get(j);
        tree_node_t * node = tree_nodes[j];
        if (aut->is_primitive && node->in_composition) {
            coin_state_t * cst = aut->states[state_vector[j]];
            candidate_input -> union_with(cst->input_act);
            candidate_output-> union_with(cst->output_act);
        }
        if (aut->is_primitive && !node->in_composition) {
            other_input -> union_with(aut->all_input_act);
            other_output -> union_with(aut->all_output_act);
        }
    }

    // step 2: check if no communication between candidates and other
    //	would be possible

    bool result = !candidate_input->common_element(other_output) &&
                !candidate_output->common_element(other_input);
    delete candidate_input;
    delete candidate_output;
    delete other_input;
    delete other_output;
    return result;
}

void Coin::setC0C2(const vector<transition_t *> * enabled_trans) {
    // clear the values
    for (int i = 0; i < automata_size; i++) {
        tree_node_t * n = tree_nodes[i];
        n->c2 = true;
        n->nonempty_succs = false;
    }

    // process simple automata affected by enabled_trans
    for (vector<transition_t *>::const_iterator i = enabled_trans->begin();
            i != enabled_trans->end(); i++) {

        transition_t * t = *i;
        int a1_id = t->get_automaton();
        int a2_id = -1;
        aut_t * automaton = automata->get(a1_id);
        aut_t * automaton2 = NULL;
        tree_node_t * node1 = tree_nodes[a1_id];
        tree_node_t * node2 = NULL;
        if (t->is_sync()) {
            a2_id = t->get_automaton2();
            automaton2 = automata->get(a2_id);
            node2 = tree_nodes[a2_id];
        }

        // check C0
        node1->nonempty_succs = true;
        if (node2) {
            node2->nonempty_succs = true;
        }

        // check C2
        // * check interesting label (Act')
        if (coin_system->int_Act.find(t->get_label()) != coin_system->int_Act.end()) {
            node1->c2 = false;
            if (node2) {
                node2->c2 = false;
            }
            continue; // no further checks needed
        }

        // * check Ap' visibility
        if (automaton->is_Ap_visible_trans(state_vector[a1_id], t->get_to_state())
                || (automaton2 && automaton2->is_Ap_visible_trans(
                        state_vector[a2_id], t->get_to_state2()))) {
            node1->c2 = false;
            if (node2) {
                node2->c2 = false;
            }
            continue; // no further checks needed
        }
    }

    // now set c2 and nonempty_succs for composite automata
    for (int i = 0; i < automata_size; i++) {
        aut_t * aut = automata->get(i);
        if (!aut->is_primitive) {
            tree_node_t * parent_node = tree_nodes[i];
            bool tmp_c2 = true;
            bool tmp_nonempty_succs = false;
            vector<int>::iterator end = aut->composed_of.end();
            for (vector<int>::iterator j = aut->composed_of.begin();
                    j != end; j++) {
                int child_id = *j;
                tree_node_t * child_node = tree_nodes[child_id];
                tmp_c2 =
                        tmp_c2 && child_node->c2;
                tmp_nonempty_succs =
                        tmp_nonempty_succs || child_node->nonempty_succs;
            }
            parent_node->c2 = tmp_c2;
            parent_node->nonempty_succs = tmp_nonempty_succs;
        }
    }
}

inline Coin::Node Coin::newNode() {
    Blob b = makeBlobCleared(State::metrics.size);
    setAmpleSet(b, -1); //newly allocated node, ample set is unknown
    return b;
}

inline Coin::Extension & Coin::extension( Node & n ) {
    return pool().get< Extension >( n, original_slack );
}

inline int Coin::getAmpleSet(Node & n) {
    return extension(n).ample_set_id;
}

inline void Coin::setAmpleSet(Node & n, int id) {
    extension(n).ample_set_id = id;
}

int Coin::findAmpleSet(vector<transition_t *> * enabled_trans) {
    aut_t * automaton;
    tree_node_t * node;
    int i;
    vector<int>::iterator j;

    // This passes through enabled_trans and sets c2 and nonempty_succs
    //  accordingly

    setC0C2(enabled_trans);

    // First iteration through simple automata
    for (i = 0; i < automata_size; i++) {
        automaton = automata->get(i);
        node = tree_nodes[i];
        if (automaton->is_primitive &&
                node->nonempty_succs && node->c2 && checkC1(i)) {
            return i;
        }
    }

    // Second iteration through composite automata
    for (i = 0; i < automata_size; i++) {
        automaton = automata->get(i);
        node = tree_nodes[i];
        if (i != system_aut_id && !automaton->is_primitive &&
                node->nonempty_succs && node->c2 && checkC1(i)) {
            return i;
        }
    }
    return system_aut_id;
}

#ifdef LCA
void Coin::markSystemComponents(int id) {
    aut_t *a = automata->get(id);
    a->is_system_component = true;
    for(vector<int>::iterator it = a->composed_of.begin(); it != a->composed_of.end(); it++) {
        markSystemComponents(*it);
    }
}

void Coin::initSystemPrimitiveComponents() {
    markSystemComponents(system_aut_id);

    system_primitive_components.clear();
    for (int i = 0; i < automata_size; i++) {
        aut_t * a = automata->get(i);
        if (a->is_primitive && a->is_system_component) {
            system_primitive_components.push_back(i);
        }
    }
/* TODO
    for (int i = 0; i < automata_size; i++) {
        aut_t * a = automata->get(i);
        if (!a->is_system_component) {
            std::cout << "Deleting unused automaton " << a->aut_name << std::endl;
            delete a;
        }
    }
*/
    system_primitive_components.resize(system_primitive_components.size());
}

int Coin::computeLCA(int i, int j) {
    aut_t * a1 = automata->get(i);
    aut_t * a2 = automata->get(j);
    aut_t * ancestor = NULL;

    for(aut_t * current = a1; current != NULL; current = current->parent) {
        current->visited = true;
    }
    for(aut_t * current = a2; current != NULL; current = current->parent) {
        if(current->visited) {
            ancestor = current;
            break;
        }
    }

    //cleanup!
    for(aut_t * current = a1; current != NULL; current = current->parent) {
        current->visited = false;
    }

    assert(ancestor != NULL);

    return automata->find_id(ancestor->aut_name);
}

void Coin::initLCA() {
    lca = (int**) malloc(system_primitive_components.size() * sizeof(int*));
    for (int i = 0; i < system_primitive_components.size(); i++) {
        lca[i] = (int*) malloc(i * sizeof(int));
        for(int j = 0; j < i; j++) {
            lca[i][j] = computeLCA(system_primitive_components[i],
                    system_primitive_components[j]);
        }
    }
}

int Coin::getLCA(int i, int j) {
    if(i == j) {
        std::cerr << "The automata must be different." << std::endl;
    }
    int x = std::max(i, j); // first coordinate contains all primitive comp.
    int y = std::min(i, j); // second coordinate contains only previous ones
    return lca[x][y];
}

label_set * Coin::computeRestrictSet(int lower, int upper) {
    label_set * const result = new label_set();

    aut_t * current = automata->get(lower);
    aut_t * last = automata->get(upper);

    do {

#ifndef NDEBUG
        if (current == NULL) {
            fprintf(stderr, "Error - current is NULL\n");
        }
#endif

        //TODO chci tady uz ten prvni automat???
        if (current->is_restrict) {
            for (set<label_t>::iterator it = current->restrictions.begin();
                    it != current->restrictions.end(); it++) {
                result->insert(*it);
            }
        }
        current = current->parent;
    } while (current != last && current != NULL);
    return result;
}

label_set * Coin::computeOnlySet(int lower, int upper) {
    label_set * result = empty_label_set;

    aut_t * current = automata->get(lower);
    aut_t * last = automata->get(upper);

    do {

#ifndef NDEBUG
        if (current == NULL) {
            fprintf(stderr, "Error - current is NULL\n");
        }
#endif

        //process just nonempty only-sets
        if (!current->is_restrict && !current->restrictions.empty()) {
            if (result == empty_label_set) {
                result = new label_set();
                for (set<label_t>::iterator it = current->restrictions.begin();
                        it != current->restrictions.end(); it++) {
                    result->insert(*it);
                }
            } else {
                for (label_set::iterator it = result->begin();
                        it != result->end(); /* empty */) {
                    if (current->restrictions.count(*it) <= 0) {
                        it = result->erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        current = current->parent;
    } while (current != last && current != NULL);

    return result;
}

void Coin::initRestrictSets() {
    empty_label_set = new label_set();

    restrict = (label_set ***) malloc(automata_size * sizeof(label_set **));
    only = (label_set ***) malloc(automata_size * sizeof(label_set **));

    for(int i = 0; i < automata_size; i++) {
        restrict[i] = (label_set **) malloc((i+1) * sizeof(label_set *));
        only[i] = (label_set **) malloc((i+1) * sizeof(label_set *));
        for(int j = 0; j <= i; j++) {
            restrict[i][j] = NULL;
            only[i][j] = NULL;
        }
    }

    for(int i = 0; i < system_primitive_components.size(); i++) {
        int a1 = system_primitive_components[i];
        if(restrict[system_aut_id][a1] == NULL) {
            restrict[system_aut_id][a1] = computeRestrictSet(a1, system_aut_id);
            only[system_aut_id][a1] = computeOnlySet(a1, system_aut_id);
        }
        for(int j = 0; j < i; j++) {
            int a2 = system_primitive_components[j];
            int lca = getLCA(i, j);

            if(restrict[lca][a1] == NULL) {
                restrict[lca][a1] = computeRestrictSet(a1, lca);
                only[lca][a1] = computeOnlySet(a1, lca);
            }
            if(restrict[lca][a2] == NULL) {
                restrict[lca][a2] = computeRestrictSet(a2, lca);
                only[lca][a2] = computeOnlySet(a2, lca);
            }
            if(restrict[system_aut_id][lca] == NULL) {
                restrict[system_aut_id][lca] = computeRestrictSet(lca, system_aut_id);
                only[system_aut_id][lca] = computeOnlySet(lca, system_aut_id);
            }
        }
    }
}

label_set * Coin::getRestrictSet(int lower, int upper) {
    int x = std::max(lower, upper);
    int y = std::min(lower, upper);
    return restrict[x][y];
}

label_set * Coin::getOnlySet(int lower, int upper) {
    int x = std::max(lower, upper);
    int y = std::min(lower, upper);
    return only[x][y];
}

bool Coin::isLabelAllowed(const label_t& l, int from, int to, bool incl) {
    label_set * r = getRestrictSet(from, to);
    assert(r != NULL);
    if(r->count(l) > 0) {
        return false;
    }

    label_set * o = getOnlySet(from, to);
    assert(o != NULL);
    if(o != empty_label_set && o->count(l) <= 0) {
        return false;
    }

    if(incl) {
        aut_t * last = automata->get(to);
        bool found = last->restrictions.count(l) > 0;
        if(last->is_restrict == found) {
            return false;
        }
    }

    return true;
}

vector<sometrans> Coin::getSuccTransitionsLCA() {
    //TODO store pointers only?
    vector<sometrans> result;

    int a1, a2, s1, s2;

    vector<trans_t *> *v1, *v2;
    vector<trans_t *>::iterator v1it, v1end, v2it, v2end;

    int N = system_primitive_components.size();

    for (int i = 0; i < N; i++) {
        a1 = system_primitive_components[i];
        s1 = state_vector[a1];

        //internal
        v1 = &(automata->get(a1)->states[s1]->internal_trans);
        for (v1it = v1->begin(), v1end = v1->end(); v1it != v1end; v1it++) {
            //printf("Internal: %d\n", it->second->tostate_id);
            trans_t * t = *v1it;
            if (isLabelAllowed(t->label, a1, system_aut_id, true)) {
                sometrans strn;
                strn.prop = false;
                strn.single = true;
                strn.aut_id = a1;
                strn.state_id = t->tostate_id;
                strn.label = t->label;
                result.push_back(strn);
            }
        }

        //external - input
        v2 = &(automata->get(a1)->states[s1]->input_trans);
        for (v2it = v2->begin(), v2end = v2->end(); v2it != v2end; v2it++) {
            //printf("Input: from state %d\n", it->second->tostate_id);
            trans_t * t = *v2it;
            if (isLabelAllowed(t->label, a1, system_aut_id, true)) {
                sometrans strn;
                strn.prop = false;
                strn.single = true;
                strn.aut_id = a1;
                strn.state_id = t->tostate_id;
                strn.label = t->label;
                result.push_back(strn);
            }
        }

        //external - output
        v1 = &(automata->get(a1)->states[s1]->output_trans);
        for (v1it = v1->begin(), v1end = v1->end(); v1it != v1end; v1it++) {
            //printf("Output: to state %d\n", it->second->tostate_id);
            trans_t * send = *v1it;
            if (isLabelAllowed(send->label, a1, system_aut_id, true)) {
                sometrans strn;
                strn.prop = false;
                strn.single = true;
                strn.aut_id = a1;
                strn.state_id = send->tostate_id;
                strn.label = send->label;
                result.push_back(strn);
            }
            //sync
            int act_id = send->label.act_id;
            for (int j = 0; j < N; j++) {
                //iterate through all the other automata
                if (j != i) {
                    a2 = system_primitive_components[j];
                    int lca = getLCA(i, j);
                    if (!isLabelAllowed(send->label, a1, lca, false)) {
                        continue;
                    }
                    s2 = state_vector[a2];
                    v2 = &(automata->get(a2)->states[s2]->input_trans);

                    //TODO Maybe we could check if the input and output transitions are enabled earlier,
                    //also the order of the checks may be optimized - e.g. source transitions first

                    for (v2it = v2->begin(), v2end = v2->end(); v2it != v2end; v2it++) {
                        trans_t * recv = *v2it; // receiver transition
                        if(recv->label.act_id == act_id) { //same action
                            if (!isLabelAllowed(recv->label, a2, lca, false)) {
                                continue;
                            }
                            label_t sync_label; //create new label
                            sync_label.act_id = act_id; //with the same action
                            sync_label.fromc_id = send->label.fromc_id; //sender component from the sender transition
                            sync_label.toc_id = recv->label.toc_id; //receiver component from the receiver transition
                            if (!isLabelAllowed(sync_label, lca, system_aut_id, true)) {
                                continue;
                            }
                            sometrans sync;
                            sync.prop = false;
                            sync.single = false; //sync transition
                            sync.aut_id = a1; //sending automaton
                            sync.aut_id2 = a2; //receiving automaton
                            sync.state_id = send->tostate_id; //sender moves to state_id
                            sync.state_id2 = recv->tostate_id; //receiver moves to state_id2
                            sync.label = sync_label; //assign the newly created label
                            result.push_back(sync);
                        }
                    }
                }
            }
        }
    }

    return result;
}
#endif

}  // namespace generator
}  // namespace divine
#endif

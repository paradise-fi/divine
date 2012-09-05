#include "combine.h"
#include "tools/combine.m4.h"
#ifdef O_LTL3BA
#include "ltl3ba/ltl3ba.h"
#endif

#ifndef O_LTL3BA
static std::string literal_to_cpp(LTL_literal_t Lit)
{
    if (Lit.negace)
        return "prop_" + Lit.predikat;
    else
        return "!prop_" + Lit.predikat;
}

/*
 * TODO: this function is a mess. We need to factor the common code generation
 * stuff out of the DVE compiler and use it here as well. In the meantime, this
 * seems to work reasonably well.
 */
std::string graph_to_cpp(const BA_opt_graph_t &g)
{
    std::stringstream s, decls;
    typedef std::list< KS_BA_node_t * > NodeList;
    typedef std::list< BA_trans_t > TransList;
    std::set< std::string > lits;

    NodeList nodes, initial, accept;
    nodes = g.get_all_nodes();
    initial = g.get_init_nodes();
    accept = g.get_accept_nodes();

    s << "#include <iostream>" << std::endl;
    s << "#include <stdio.h>" << std::endl;

    s << "int *buchi_state( CustomSetup *setup, char *st ) {" << std::endl;
    s << "    return (int *)(st + setup->slack + 4 /* FIXME BlobHeader */ - sizeof(int));"
      << std::endl;
    s << "}" << std::endl << std::endl;

    s << "extern \"C\" int is_accepting( CustomSetup *setup, char *st, int ) {" << std::endl;
    s << "    int buchi_st = *buchi_state( setup, st ) + 1;" << std::endl;
    for ( NodeList::const_iterator n = accept.begin(); n != accept.end(); ++n )
        s << "    if ( buchi_st == " << (*n)->name << " ) return true;" << std::endl;
    s << "    return false;" << std::endl;
    s << "}" << std::endl;
    s << std::endl;

    s << "extern \"C\" int setup( CustomSetup *setup ) {" << std::endl;
    s << "    setup->slack += sizeof(int);" << std::endl;
    s << "    system_setup( setup );" << std::endl;
    s << "}" << std::endl;

    s << "extern \"C\" void get_initial( CustomSetup *setup, char **to ) {" << std::endl;
    s << "    get_system_initial( setup, to );" << std::endl;
    s << "    *(int *)((*to) + setup->slack - sizeof(int)) = 0;" << std::endl;
    s << "}" << std::endl;

    s << "extern \"C\" char *show_node( CustomSetup *setup, char *st, int l ) {" << std::endl;
    s << "    char *sys = system_show_node( setup, st, l );" << std::endl;
    s << "    char *bonk = (char *)malloc( strlen( sys ) + 32 );" << std::endl;
    s << "    sprintf( bonk, \"[LTL: %d] %s\", *buchi_state( setup, st ) + 1, sys );" << std::endl;
    s << "    return bonk;" << std::endl;
    s << "}" << std::endl;

    s << "extern \"C\" int get_successor( CustomSetup *setup, int next, char *from, char **to ) {" << std::endl;
    s << "    int buchi_next = next >> 24;" << std::endl;
    s << "    int system_next = next & 0xffffff;" << std::endl;
    s << "    int buchi_st = *buchi_state( setup, from ) + 1;" << std::endl;
    int buchi_next = 0;
    for ( NodeList::const_iterator n = nodes.begin(); n != nodes.end(); ++n ) {
        for ( TransList::const_iterator t = g.get_node_adj(*n).begin(); t != g.get_node_adj(*n).end(); ++t ) {
            s << "    if ( buchi_next == " << buchi_next << " ) {" << std::endl;
            s << "        if ( buchi_st == " << (*n)->name;
            for ( LTL_label_t::const_iterator l = t->t_label.begin(); l != t->t_label.end(); ++l ) {
                lits.insert( lits.begin(), l->predikat );
                s << " && " << literal_to_cpp( *l ) << "( setup, from )";
            }
            s << " )" << std::endl;
            s << "            system_next = get_system_successor( setup, system_next, from, to );" << std::endl;
            s << "        else system_next = 0;" << std::endl;
            s << "        if (system_next) {" << std::endl;
            s << "            *buchi_state( setup, *to ) = " << t->target->name - 1 << ";" << std::endl;
            s << "            return (system_next & 0xffffff) | (buchi_next << 24);" << std::endl;
            s << "        }" << std::endl;
            s << "        system_next = 1;" << std::endl;
            s << "        ++ buchi_next; /* done with this one, try next buchi edge */" << std::endl;
            s << "    }" << std::endl;
            ++ buchi_next;
        }
    }

    s << "    return 0;" << std::endl;
    s << "}" << std::endl;

    decls << "extern \"C\" int get_system_successor( CustomSetup *, int, char *, char ** );" << std::endl;
    decls << "extern \"C\" void get_system_initial( CustomSetup *setup, char **to );" << std::endl;
    decls << "extern \"C\" char *system_show_node( CustomSetup *setup, char *from, int );" << std::endl;
    decls << "extern \"C\" bool prop_limit_active( CustomSetup *setup, char *st );" << std::endl;
    decls << "extern \"C\" void system_setup( CustomSetup *setup );" << std::endl;

    for ( std::set< std::string >::iterator i = lits.begin(); i != lits.end(); ++ i )
        decls << "extern \"C\" bool prop_" << *i << "( CustomSetup *setup, char *st );" << std::endl;

    return decls.str() + "\n" + s.str();
}
#endif

#ifdef O_LTL3BA
void print_buchi_trans_label(bdd label, std::ostream& out) {
    std::stringstream s;
	  while (label != bddfalse) {
        bdd current = bdd_satone(label);
        label -= current;

        out << "(";
        while (current != bddtrue && current != bddfalse) {
            bdd high = bdd_high(current);
            if (high == bddfalse) {
                out << "!prop_" << get_buchi_symbol(bdd_var(current)) << "( setup, from )";
                current = bdd_low(current);
            } else {
                out << "prop_" << get_buchi_symbol(bdd_var(current)) << "( setup, from )";;
                current = high;
            }
            if (current != bddtrue && current != bddfalse) out << " && ";
        }
        out << ")";
        if (label != bddfalse) out << " || ";
    }
} 

std::string buchi_to_cpp(BState* bstates, int accept, std::list< std::string > symbols)
{
    std::stringstream s, decls;
    typedef std::map<BState*, bdd> TransMap;

    BState *n;
    int nodes = 1;

    s << "#include <iostream>" << std::endl;

    s << "int *buchi_state( CustomSetup *setup, char *st ) {" << std::endl;
    s << "    return (int *)(st + setup->slack + 4 /* FIXME BlobHeader */ - sizeof(int));"
      << std::endl;
    s << "}" << std::endl << std::endl;

    s << "extern \"C\" int is_accepting( CustomSetup *setup, char *st, int ) {" << std::endl;
    s << "    int buchi_st = *buchi_state( setup, st ) + 1;" << std::endl;
    for ( n = bstates->prv; n != bstates; n = n->prv ) {
        n->incoming = nodes++;
        if ( n->final == accept )
            s << "    if ( buchi_st == " << n->incoming << " ) return true;" << std::endl;
    }
    s << "    return false;" << std::endl;
    s << "}" << std::endl;
    s << std::endl;

    s << "extern \"C\" int setup( CustomSetup *setup ) {" << std::endl;
    s << "    setup->slack += sizeof(int);" << std::endl;
    s << "    system_setup( setup );" << std::endl;
    s << "}" << std::endl;

    s << "extern \"C\" void get_initial( CustomSetup *setup, char **to ) {" << std::endl;
    s << "    get_system_initial( setup, to );" << std::endl;
    s << "    *(int *)((*to) + setup->slack - sizeof(int)) = 0;" << std::endl;
    s << "}" << std::endl;

    s << "extern \"C\" char *show_node( CustomSetup *setup, char *st, int l ) {" << std::endl;
    s << "    char *sys = system_show_node( setup, st, l );" << std::endl;
    s << "    char *bonk = (char *)malloc( strlen( sys ) + 32 );" << std::endl;
    s << "    sprintf( bonk, \"[LTL: %d] %s\", *buchi_state( setup, st ) + 1, sys );" << std::endl;
    s << "    return bonk;" << std::endl;
    s << "}" << std::endl;

    s << "extern \"C\" int get_successor( CustomSetup *setup, int next, char *from, char **to ) {" << std::endl;
    s << "    int buchi_next = next >> 24;" << std::endl;
    s << "    int system_next = next & 0xffffff;" << std::endl;
    s << "    int buchi_st = *buchi_state( setup, from ) + 1;" << std::endl;
    int buchi_next = 0;
    for ( n = bstates->prv; n != bstates; n = n->prv ) {
        for ( TransMap::iterator t = n->trans->begin(); t != n->trans->end(); ++t ) {
            s << "    if ( buchi_next == " << buchi_next << " ) {" << std::endl;
            s << "        if ( buchi_st == " << n->incoming;
            if (t->second != bdd_true()) {
                s << " && ";
                print_buchi_trans_label(t->second, s);
            }
            s << " )" << std::endl;
            s << "            system_next = get_system_successor( setup, system_next, from, to );" << std::endl;
            s << "        else system_next = 0;" << std::endl;
            s << "        if (system_next) {" << std::endl;
            s << "            *buchi_state( setup, *to ) = " << t->first->incoming - 1 << ";" << std::endl;
            s << "            return (system_next & 0xffffff) | (buchi_next << 24);" << std::endl;
            s << "        }" << std::endl;
            s << "        system_next = 1;" << std::endl;
            s << "        ++ buchi_next; /* done with this one, try next buchi edge */" << std::endl;
            s << "    }" << std::endl;
            ++ buchi_next;
        }
    }

    s << "    return 0;" << std::endl;
    s << "}" << std::endl;

    decls << "extern \"C\" int get_system_successor( CustomSetup *, int, char *, char ** );" << std::endl;
    decls << "extern \"C\" void get_system_initial( CustomSetup *setup, char **to );" << std::endl;
    decls << "extern \"C\" char *system_show_node( CustomSetup *setup, char *from, int );" << std::endl;
    decls << "extern \"C\" bool prop_limit_active( CustomSetup *setup, char *st );" << std::endl;
    decls << "extern \"C\" void system_setup( CustomSetup *setup );" << std::endl;

    for ( std::list< std::string >::iterator i = symbols.begin(); i != symbols.end(); ++ i )
        decls << "extern \"C\" bool prop_" << *i << "( CustomSetup *setup, char *st );" << std::endl;

    return decls.str() + "\n" + s.str();
}
#endif

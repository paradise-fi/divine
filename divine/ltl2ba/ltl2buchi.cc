/*
*  Transformation LTL formulae to Buchi automata
*
*  Main program - variant 1
*
*  Milan Jaros - xjaros1@fi.muni.cz
*
*/

#include <cstdlib>
#include <sstream>
#include <iostream>
#include <string>
#include <fstream>
#include <list>
#include <map>
#include <iterator>
#include <cctype>
#include <unistd.h>

#include "ltl.hh"
#include "formul.hh"
#include "ltl_graph.hh"
#include "opt_buchi.hh"
#include "support_dve.hh"

#include "buchi_lang.hh"

using namespace divine;
using namespace std;

void run_usage(void)
{
#if BUCHI_LANG == BUCHI_LANG_CZ
	cout << "Pouziti:" << endl;
	cout << "-h - vypise napovedu k programu" << endl;
	cout << "-r (0, 1, 2, 3, 4) - aplikace prepisovacich pravidel:" <<
		endl;
	cout << "\t0 - neaplikovat zadna pravidla" << endl;
	cout << "\t1 - aplikovat sadu pravidel 1" << endl;
	cout << "\t2 - aplikovat sadu pravidel 2" << endl;
	cout << "\t3 - aplikovat obe sady v poradi 1, 2 (defaultne)" <<
		endl;
	cout << "\t4 - aplikovat obe sady v poradi 2, 1" << endl;
	cout << "-g - negovat zadanou LTL formuli" << endl;
	cout << "-d - degeneralizovat vystupni automat" << endl;
	cout << "-O - stupen - optimalizace automatu" << endl;
	cout << "\t musi byt pouzit prepinac '-d'" << endl;
	cout << "-o (0, 1, 2) - format vystupu ";
	cout << "(neni-li pouzit prepinac '-f')" << endl;
	cout << "\t0 - format DVE (defaultne)" << endl;
	cout << "\t1 - format s olabelovanymi stavy "
		"(pouze pro GBA a BA bez optimalizaci)" << endl;
	cout << "\t2 - format s labely na hranach "
		"(pouze pro BA s pripadnymi optimalizacemi)" << endl;

	cout << "-t - jednoduchy vypis" << endl;
	cout << "-l soubor - nacte LTL formule ze souboru" << endl;
	cout << "-f soubor[.dve] - vystup ulozi do souboru "
		"'soubor.prop.dve'" << endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
	cout << "Usage:" << endl;
	cout << "-h - print help" << endl;
	cout << "-r (0, 1, 2, 3, 4) - apply rewriting rules:" <<
		endl;
	cout << "\t0 - do not apply any rules" << endl;
	cout << "\t1 - apply first set of rules" << endl;
	cout << "\t2 - apply second set of rules" << endl;
	cout << "\t3 - apply both sets of rules in sequence 1, 2 "
		"(default)" << endl;
	cout << "\t4 - apply both sets of rules in sequence 2, 1" << endl;
	cout << "-g - negate LTL formula before translation" << endl;
	cout << "-d - compute Buchi automaton, not only generalized "
	"automaton" << endl;
	cout << "-O level - optimizing Buchi automaton" << endl;
	cout << "\t must be used option '-d'" << endl;
	cout << "-o (0, 1, 2) - output format ";
	cout << "(when option '-f' were not used)" << endl;
	cout << "\t0 - DiVinE format (default)" << endl;
	cout << "\t1 - format with labels in states "
		"(only for GBA and BA without optimization)" << endl;
	cout << "\t2 - format with labeled transitions "
		"(for BA with eventualy optimizations)" << endl;

	cout << "-t - strict output" << endl;
	cout << "-l file - read LTL formula from file" << endl;
	cout << "-f file[.dve] - write output to DiVinE source file "
		"'file.prop.dve'" << endl;

#endif
}

void run_help(void)
{
	run_usage();

	cout << endl;
#if BUCHI_LANG == BUCHI_LANG_CZ
	cout << "Syntaxe formule LTL:" << endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
	cout << "Syntax of LTL formula:" << endl;
#endif
	cout << "  F :== un_op F | F bin_op F | (F) | term" << endl;
	cout << "  un_op :== '!' (negace)" << endl;
	cout << "        :== 'X' | 'O' (next)" << endl;
	cout << "        :== 'F' | '<>' (true U 'argument')" << endl;
	cout << "        :== 'G' | '[]' (!F!'argument')" << endl;
	cout << "  bin_op :== '&&' | '*' (and)" << endl;
	cout << "         :== '||' | '+' (or)" << endl;
#if BUCHI_LANG == BUCHI_LANG_CZ
	cout << "         :== '->' (implikace)" << endl;
	cout << "         :== '<->' (ekvivalence)" << endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
	cout << "         :== '->' (implication)" << endl;
	cout << "         :== '<->' (equivalention)" << endl;
#endif
	cout << "         :== '^' (xor)" << endl;
	cout << "         :== 'U' (until)" << endl;
	cout << "         :== 'V' | 'R' (release)" << endl;
	cout << "         :== 'W' (weak until)" << endl;
	cout << "  term :== 'true' | 'false'" << endl;
	cout << "       :== str" << endl;
#if BUCHI_LANG == BUCHI_LANG_CZ  
	cout << "           str je retezec, slozeny z malych pismen," <<
		"cislic ";
	cout << endl;
	cout << "           a znaku \'_\', zacinajici pismenem" <<
		" nebo znakem \'_\'" << endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
	cout << "           str is string contains low characters, " <<
		"numbers ";
		cout << endl;
	cout << "           and character \'_\', begining character a - z"
		" or \'_\'" << endl;
#endif
}


void copy_graph(const ltl_graph_t& dG, BA_opt_graph_t& oG)
{
	list<ltl_ba_node_t*> LN;
	map<int, ltl_ba_node_t*>::const_iterator ln_b, ln_e, ln_i;
	list<ltl_ba_node_t*>::const_iterator l_b, l_e, l_i;
	KS_BA_node_t *p_N;
	int max_node_name = 0;

	oG.clear();

	LN = dG.get_ba_init_nodes();
	if (LN.size() == 0) return;

	ln_b = dG.get_all_ba_nodes().begin();
	ln_e = dG.get_all_ba_nodes().end();

	for (ln_i = ln_b; ln_i != ln_e; ln_i++) {
		if (ln_i->first > max_node_name)
			max_node_name = ln_i->first;
	}

	p_N = oG.add_node(max_node_name + 1); p_N->initial = true;
	l_b = LN.begin(); l_e = LN.end();
	for (l_i = l_b; l_i != l_e; l_i++) {
		oG.add_trans(p_N, (*l_i)->name, (*l_i)->n_label);

		//if ((*l_i)->accept) p_N->accept = true;
	}

	for (ln_i = ln_b; ln_i != ln_e; ln_i++) {
		p_N = oG.add_node(ln_i->second->name);
		p_N->accept = ln_i->second->accept;
		l_b = ln_i->second->adj.begin();
		l_e = ln_i->second->adj.end();
		for (l_i = l_b; l_i != l_e; l_i++) {
			oG.add_trans(p_N, (*l_i)->name, (*l_i)->n_label);
		}
	}
}


int ltl2buchi(int argc, char **argv)
{
	string s, s1, jmeno_ltl = "", jmeno_dve = "", one_ltl = "";
	LTL_parse_t L;
	LTL_formul_t F, F1;
	ltl_graph_t G;

	int c, i;
	bool deg_graph = false, negace = false, out_props;
	bool strict_output = false;
	int optimize = -1, out_format = 0, rewrite_set = 3;

	list<LTL_formul_t> L_formul;
	list<map<string, string> > L_prop;
	list<LTL_formul_t>::iterator lf_b, lf_e, lf_i;
	list<map<string, string> >::iterator pr_b, pr_e, pr_i, pr_pom;

	while ((c = getopt(argc, argv,
		"r:-gdtO:-f:-l:-o:-hn:-")) != -1) {
		switch (c) {
		case 'r': // prepisovani
			rewrite_set = atoi(optarg);
			if (rewrite_set < 0) rewrite_set = 0;
			if (rewrite_set > 3) rewrite_set = 3;
			break;
		case 'l': // nacteni LTL formuli ze souboru
			jmeno_ltl = optarg;
			break;
		case 'n': // nacteni (jedine) LTL formule ze souboru
			one_ltl = optarg;
			break;
		case 'h': // napoveda
			run_help();
			exit(0);
			break;
		case 'g': // negovat formuli
			negace = true;
			break;
		case 'd': // degeneralizovat graf
			deg_graph = true;
			break;
		case 'O': // optimalizace
			optimize = atoi(optarg);
			break;
		case 'f': // dve soubor
			jmeno_dve = optarg;
			break;
		case 'o': // format vystupu
			out_format = atoi(optarg);
			if (out_format < 0) out_format = 0;
			if (out_format > 2) out_format = 2;
			break;
		case 't': // jednoduchy vypis
			strict_output = true;
			break;
		case '?':
			run_usage();
			exit(9);
			break;
		}
	}

	if ((jmeno_ltl != "") && (one_ltl != "")) {
#if BUCHI_LANG == BUCHI_LANG_CZ
		cerr << "Lze pouzit nejvyse jeden z prepinacu '-l', '-n'";
		cerr << endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
		cerr << "You can use only one of option '-l', '-n'" <<
		endl;
#endif
		return(2);
	}

	if (jmeno_ltl != "") {
		list<LTL_parse_t> L_ltl;
		list<LTL_parse_t>::iterator l_b, l_e, l_i;
		bool b;

		fstream(fr);
		fr.open(jmeno_ltl.c_str(), ios::in);
		if (!fr) {
#if BUCHI_LANG == BUCHI_LANG_CZ
			cerr << "Soubor '" << jmeno_ltl << "' neexistuje";
			cerr << endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
			cerr << "File '" << jmeno_ltl <<
			"' doesn't exist" << endl;
#endif
			return 2;
		}

		b = read_ltl_file(fr, L_ltl, L_prop);
		fr.close();

		if (!b) {
#if BUCHI_LANG == BUCHI_LANG_CZ
			cerr << "Soubor '" << jmeno_ltl << "' nema "
			"spravny format" << endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
			cerr << "File '" << jmeno_ltl <<
			"' doesn't have correct format" << endl;
#endif
			return(3);
		}

		l_b = L_ltl.begin(); l_e = L_ltl.end();
		pr_b = L_prop.begin(); pr_e = L_prop.end();
		for (l_i = l_b, pr_i = pr_b; l_i != l_e; l_i++, pr_i++) {
			if (l_i->syntax_check(F)) {
				L_formul.push_back(F);
				pr_i++;
			} else {
#if BUCHI_LANG == BUCHI_LANG_CZ
				cerr << "Nejde o LTL formuli: " <<
#elif BUCHI_LANG == BUCHI_LANG_EN
				cerr << "This is not LTL formula: " <<
#endif
				(*l_i) << endl;
				pr_pom = pr_i; pr_i++;
				L_prop.erase(pr_pom);
			}
		}
		out_props = true;
	} else {
		if (one_ltl != "") {
			fstream(fr);

			fr.open(one_ltl.c_str(), ios::in);
			if (!fr) {
#if BUCHI_LANG == BUCHI_LANG_CZ
				cerr << "Soubor '" << one_ltl <<
				"' neexistuje" << endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
				cerr << "File '" << jmeno_ltl <<
				"' doesn't exist" << endl;
#endif
				return(2);
			}

			L.nacti(fr, cout);
			fr.close();
		} else {
			if (strict_output) {
				cin >> L;
			} else {
				L.nacti();
			}
		}

		if (L.syntax_check(F)) {
			L_formul.push_back(F);
		} else {
#if BUCHI_LANG == BUCHI_LANG_CZ
			cerr << "Toto neni LTL formule" << endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
			cerr << "This is not LTL formula" << endl;
#endif
			return 1;
		}
		out_props = false;
	}


	lf_b = L_formul.begin(); lf_e = L_formul.end();
	pr_b = L_prop.begin(); pr_e = L_prop.end();
	i = 1;
	for (lf_i = lf_b, pr_i = pr_b; lf_i != lf_e; lf_i++, i++) {
		reg_ltl_at_props(0);
		if (negace) F = lf_i->negace();
		else F = *lf_i;

		if (!strict_output && !out_props) {
#if BUCHI_LANG == BUCHI_LANG_CZ
			cout << "Formule v normalni forme:" << endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
			cout << "Formula in positive normal form:" <<
			endl;
#endif
			cout << F << endl << endl;
		}

		switch (rewrite_set) {
		case 1: F.rewrite(F1); F = F1;
			break;
		case 2: F.rewrite2(F1); F = F1;
			break;
		case 3: F.rewrite(F1); F1.rewrite2(F);
			break;
		case 4: F.rewrite2(F1); F1.rewrite(F);
			break;
		}

		if (!strict_output && !out_props && (rewrite_set != 0)) {
#if BUCHI_LANG == BUCHI_LANG_CZ
			cout << "Prepsana formule:" << endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
			cout << "Rewriten formula:" << endl;
#endif
			cout << F << endl << endl;
		}

		ostringstream oss;
		oss <<F;
		if (oss.str() == "false")
		  {
		    optimize = 0;
		    deg_graph=false;
		    if (!strict_output && !out_props)
		      {
#if BUCHI_LANG == BUCHI_LANG_CZ
			cout << "Nebudu provadet optimalizace."<<endl<<endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
			cout <<"Optimizations turned off."<<endl<<endl;
#endif
		      }

		  }

		G.create_graph(F);
		if (deg_graph) {
			BA_opt_graph_t oG, oG1;

			G.degeneralize();
			copy_graph(G, oG);

			oG.optimize(oG1, optimize);

			if (jmeno_dve != "") {
				if (out_props) {
					reg_ltl_at_props(&(*pr_i));
					output_to_file(oG1, jmeno_dve, i);
#if BUCHI_LANG == BUCHI_LANG_CZ
					cerr << "Zapsana formule c." <<
					i << endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
					cerr << "writing formula no. " <<
						i << endl;
#endif
				} else {
					output_to_file(oG1, jmeno_dve);
				}
			} else {
				if (out_props) {
					reg_ltl_at_props(&(*pr_i));
				} else if (!strict_output) {
#if BUCHI_LANG == BUCHI_LANG_CZ
					cout << "Buchiho automat:" <<
						endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
					cout << "Buchi automaton:" <<
						endl;
#endif
				}
				if (out_format == 0)
					output(oG1, cout);
				if (out_format == 1)
					G.vypis_BA(strict_output);
				if (out_format == 2) {
					oG1.vypis(strict_output);
					if (strict_output) cout << "# ";
					switch (oG1.get_aut_type()) {
					case BA_STRONG:
						cout << "strong"; break;
					case BA_WEAK:
						cout << "weak"; break;
					case BA_TERMINAL:
						cout << "terminal"; break;
					}
					cout << endl;
				}
			}
		} else {
			if (jmeno_dve != "") {
				if (out_props) {
					reg_ltl_at_props(&(*pr_i));
					output_to_file(G, jmeno_dve, i);
#if BUCHI_LANG == BUCHI_LANG_CZ
					cerr << "Zapsana formule c." <<
					i << endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
					cerr << "writing formula no. " <<
					i << endl;
#endif
				} else {
					output_to_file(G, jmeno_dve);
				}
			} else {
				if (out_props) {
					reg_ltl_at_props(&(*pr_i));
				} else if (!strict_output) {
#if BUCHI_LANG == BUCHI_LANG_CZ
					cout << "Buchiho automat:" <<
						endl;
#elif BUCHI_LANG == BUCHI_LANG_EN
					cout << "Buchi automaton:" <<
						endl;
#endif
				}
				if (out_format == 0) {
					output(G, cout);
				} else {
					G.vypis_GBA();
				}
			}
		}

		if (out_props) pr_i++;
	}

	return 0;
}

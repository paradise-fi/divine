/*
*  Transformation LTL formulae to Buchi automata
*    - implementation of helping functions
* 
*  Milan Jaros - xjaros1@fi.muni.cz
* 
*/

#include <cstdio>

#include "support_dve.hh"

//#include "error/error.hh"
//#include "deb/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
using namespace std;
#endif //DOXYGEN_PROCESSING

map<string, string> *at_props = 0; // atomicke propozice

void divine::reg_ltl_at_props(map<string, string> *p_AP) {
	at_props = p_AP;
}

bool divine::read_ltl_file(istream& fr, list<LTL_parse_t>& L_ltl,
	list<map<string, string> >& L_prop)
{
	if (!L_ltl.empty())
		L_ltl.erase(L_ltl.begin(), L_ltl.end());
	if (!L_prop.empty())
		L_prop.erase(L_prop.begin(), L_prop.end());

	int stav = 0;
	string s, p1, p2;;
	char c;
	LTL_parse_t L;
	map<string, string> M;

	while (!fr.eof()) {
		switch (stav) {
		case 0: // mimo formuli
			getline(fr, s, '{');
			if (fr.eof()) return(true);
			getline(fr, s, '{');
			getline(fr, s, '}');
			L.nacti(s);
			M.erase(M.begin(), M.end());
			stav = 1; break;
		case 1: // nacitani propozic
			do { fr >> c;
			} while ((!fr.eof()) && isspace(c));
			if (fr.eof()) return(false);
			if (c == '}') {
				L_ltl.push_back(L);
				L_prop.push_back(M);
				stav = 0; break;
			}
			p1 = c; fr >> c;
			while (!fr.eof() && (c != ':') && (!isspace(c))) {
				p1 += c; fr >> c;
			} if (c != ':') getline(fr, s, ':');
			if (fr.eof()) return(false);
			fr >> c;
			if ((c != '=') || fr.eof()) return(false);
			fr >> c;
			while (!fr.eof() && isspace(c)) fr >> c;
			if (fr.eof()) return(false);
			p2 = c;
			getline(fr, s, ';');
			if (fr.eof()) return(false);
			p2 += s;
			M[p1] = p2;
		}
	}

	if (stav == 0)
		return(true);
	else
		return(false);
}

string print_formul(LTL_formul_t F)
{
	bool neg;
	string prop, s = "";
	map<string, string>::iterator p_i;

	if (F.get_literal(neg, prop)) {
		if (!neg) s = "not ";
		if (at_props != 0) {
			p_i = at_props->find(prop);
			if (p_i == at_props->end()) {
				s += prop;
			} else {
				if (!neg) s += "(";
				s += p_i->second;
				if (!neg) s += ")";
			}
		} else {
			s += prop;
		}
		return(s);
	} else {
		return("");
	}
}

string print_literal(LTL_literal_t Lit)
{
	string s = "";
	map<string, string>::iterator p_i;

	if (!Lit.negace) s = "not ";
	if (at_props != 0) {
		p_i = at_props->find(Lit.predikat);
		if (p_i == at_props->end()) {
			s += Lit.predikat;
		} else {
			if (!Lit.negace) s += "(";
			s += p_i->second;
			if (!Lit.negace) s += ")";
		}
	} else {
		s += Lit.predikat;
	}
	return(s);
}

bool open_file(string file_name, fstream& fr, fstream& fw, int n = 0)
{
	string jm_r, jm_w;
	string pripona_fw;
	int l;

	if (n == 0) {
		pripona_fw = ".prop.dve";
	} else {
		char pom[10];
		sprintf(pom, "%d", n);
		pripona_fw = ".prop";
		pripona_fw += pom;
		pripona_fw += ".dve";
	}

	l = file_name.length();

	if (l > 4) {
		if ((file_name[l-4] == '.') && (file_name[l-3] == 'd') &&
		(file_name[l-2] == 'v') && (file_name[l-1] == 'e')) {
			jm_r = file_name;
			file_name.erase(l-4);
			jm_w = file_name + pripona_fw;
		} else {
			jm_r = file_name + ".dve";
			jm_w = file_name + pripona_fw;
		}
	} else {
		jm_r = file_name + ".dve";
		jm_w = file_name + pripona_fw;
	}

	fr.open(jm_r.c_str(), ios::in);
	if (!fr) {
		cerr << "Soubor " << jm_r << " neexistuje" << endl;
		return(false);
	}

	fw.open(jm_w.c_str(), ios::out);
	if (!fw) {
		fr.close();
		cerr << "Do souboru " << jm_w << " nelze zapisovat" <<
			endl;
		return(false);
	}

	return(true);
}

bool copy_file(istream& fr, ostream& fw)
{
	string s;
	int stav = 0, i;

	int npos = string::npos;

	while (!fr.eof()) {
		getline(fr, s);
		switch (stav) {
		case 0: // mimo definici procesu
			i = s.find("process");
			if (i != npos) {
				i = s.find("{");
				if (i != npos) {
					stav = 1;
				} else {
					stav = 2;
				}
				fw << s << endl;
				break;
			}
			i = s.find("system");
			if (i == npos) {
				fw << s << endl;
			}
			break;
		case 1: // 
			i = s.find("{");
			if (i != npos) {
				stav = 2;
			}
			fw << s << endl;
			break;
		case 2: // v definici procesu
			i = s.find("{");
			if (i != npos) {
				i = s.find("}");
				if (i == npos) {
					stav = 3;
				}
			} else {
				i = s.find("}");
				if (i != npos) {
					stav = 0;
				}
			}
			fw << s << endl;
			break;
		case 3: // v definici procesu - zavorka }
			i = s.find("}");
			if (i != npos) {
				stav = 2;
			}
			fw << s << endl;
			break;
		}
	}

	return(true);
}

#ifndef CC_ONLY_OPT
void divine::output_to_file(const ltl_graph_t& G, string file_name, int n)
{
	fstream(fr); fstream(fw);
	if (!open_file(file_name, fr, fw, n)) {
		return;
	}

	if (!copy_file(fr, fw)) {
		return;
	}

	output(G, fw);
	fw << endl << "system async property LTL_property;" << endl;

	fr.close(); fw.close();
}

#endif

void divine::output_to_file(const BA_graph_t& G, string file_name, int n)
{
	fstream(fr); fstream(fw);
	if (!open_file(file_name, fr, fw, n)) {
		return;
	}

	if (!copy_file(fr, fw)) {
		return;
	}

	output(G, fw);
	fw << endl << "system async property LTL_property;" << endl;

	fr.close(); fw.close();
}

#ifndef CC_ONLY_OPT
void divine::output(const ltl_graph_t& G, ostream& fw)
{
	list<ltl_gba_node_t> LN;
	list<ltl_gba_node_t>::const_iterator ln_b, ln_e, ln_i;

	list<int> LN_all;
	list<int>::const_iterator l_b, l_e, l_i;

	LTL_set_acc_set_t::const_iterator sas_b, sas_e, sas_i;
	LTL_acc_set_t::const_iterator as_b, as_e, as_i;

	LTL_label_t LL;
	LTL_label_t::const_iterator lf_b, lf_e, lf_i;

	fw << "process LTL_property {" << endl;

	fw << "state q0";
	G.get_all_gba_nodes(LN_all);
	l_b = LN_all.begin(); l_e = LN_all.end();
	for (l_i = l_b; l_i != l_e; l_i++) {
		fw << ", q" << *l_i;
	}
	fw << ";" << endl;

	fw << "init q0;" << endl;

	sas_b = G.get_gba_set_acc_set().begin();
	sas_e = G.get_gba_set_acc_set().end();
	for (sas_i = sas_b; sas_i != sas_e; sas_i++) {
		fw << "accept ";
		as_b = sas_i->begin(); as_e = sas_i->end();
		fw << "q" << (*as_b);
		as_i = as_b; as_i++;
		for ( ; as_i != as_e; as_i++) {
			fw << ", q" << (*as_i);
		}
		fw << ";" << endl;
	}

	

	bool b;

	G.get_gba_init_nodes(LN);
	ln_b = LN.begin(); ln_e = LN.end(); b = false;
	for (ln_i = ln_b; ln_i != ln_e; ln_i++) {
		if (b) {
			fw << "," << endl;
		} else {
		  fw << "trans" << endl;		  
		  b = true;
		}
		fw << "q0 -> q" << ln_i->name << " {";
		ln_i->get_label(LL);
		if (!LL.empty()) {
			lf_b = LL.begin(); lf_e = LL.end();
			fw << " guard " << print_literal(*lf_b);
			lf_i = lf_b; lf_i++;
			for ( ; lf_i != lf_e; lf_i++) {
				fw << " && " << print_literal(*lf_i);
			}
			fw << "; ";
		}
		fw << "}";
	}

	for (l_i = l_b; l_i != l_e; l_i++) {
		G.get_gba_node_adj(*l_i, LN);
		ln_b = LN.begin(); ln_e = LN.end();
		for (ln_i = ln_b; ln_i != ln_e; ln_i++) {
			fw << "," << endl;
			fw << "q" << *l_i << " -> q" <<
			ln_i->name << " {";
			ln_i->get_label(LL);
			if (!LL.empty()) {
				lf_b = LL.begin(); lf_e = LL.end();
				fw << " guard " << print_literal(*lf_b);
				lf_i = lf_b; lf_i++;
				for ( ; lf_i != lf_e; lf_i++) {
					fw << " && " <<
					print_literal(*lf_i);
				}
				fw << "; ";
			}
			fw << "}";
		}
	}
	
	if (b)
	  fw << ";" << endl;

	fw << "}" << endl;
}

#endif

void divine::output(const BA_graph_t& G, ostream& fw)
{
	list<KS_BA_node_t*> LN_all, LN;
	list<KS_BA_node_t*>::const_iterator ln_b, ln_e, ln_i,
		l_b, l_e, l_i;
	list<BA_trans_t>::const_iterator t_b, t_e, t_i;
	const KS_BA_node_t *p_N;

	LTL_label_t::const_iterator lf_b, lf_e, lf_i;

	bool b;

	LN = G.get_init_nodes();
	if (LN.size() != 1) {
		cerr << "Graf ma " << LN.size() << " inicialnich stavu";
		cerr << endl;
		return;
	}

	fw << "process LTL_property {" << endl;

	p_N = LN.front();

	fw << "state "; b = false;
	LN_all = G.get_all_nodes();
	ln_b = LN_all.begin(); ln_e = LN_all.end();
	for (ln_i = ln_b; ln_i != ln_e; ln_i++) {
		if (b) fw << ", ";
		else b = true;
		fw << "q" << (*ln_i)->name;
	}
	fw << ";" << endl;

	fw << "init q" << p_N->name << ";" << endl;

	LN = G.get_accept_nodes();
	l_b = LN.begin(); l_e = LN.end(); b = false;

	if (l_b != l_e)
	  fw << "accept ";
	for (l_i = l_b; l_i != l_e; l_i++) {
		if (b) {
			fw << ", ";
		} else {
			b = true;
		}
		fw << "q" << (*l_i)->name;
	}
	if (l_b != l_e)
	  fw << ";" << endl;

	b = false;

	for (ln_i = ln_b; ln_i != ln_e; ln_i++) {
		p_N = *ln_i;
		t_b = G.get_node_adj(p_N).begin();
		t_e = G.get_node_adj(p_N).end();
		for (t_i = t_b; t_i != t_e; t_i++) {
		        if (!b)	fw << "trans" << endl; 
			if (b) fw << "," << endl;
			else b = true;
			fw << "q" << p_N->name << " -> q" << 
				t_i->target->name << " {";
			if ( !(t_i->t_label.empty()) ) {
				lf_b = t_i->t_label.begin();
				lf_e = t_i->t_label.end();
				fw << " guard " << print_literal(*lf_b);
				lf_i = lf_b; lf_i++;
				for ( ; lf_i != lf_e; lf_i++) {
					fw << " && " <<
					print_literal(*lf_i);
				}
				fw << "; ";
			}
			fw << "}";
		}
	}
	if (b)	  
	  fw << ";" << endl;

	fw << "}" << endl;
}

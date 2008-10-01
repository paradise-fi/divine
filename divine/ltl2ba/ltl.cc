/*
*  Syntax of LTL formulae, translation to instance of class LTL_formul_t
*    - implementation
*
*  Milan Jaros - xjaros1@fi.muni.cz
*
*/

#include <stack>
#include <string>
#include "ltl.hh"
#include "formul.hh"

#include <sevine.h>

//#include "error/error.hh"
//#include "deb/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace std;
using namespace divine;
#endif //DOXYGEN_PROCESSING

LTL_parse_t::LTL_parse_t():formule("")
{ }     

LTL_parse_t::LTL_parse_t(const LTL_parse_t& a):formule(a.formule)
{ }
 
LTL_parse_t::LTL_parse_t(string& s):formule(s)
{ }

LTL_parse_t::~LTL_parse_t()
{ }
        
void LTL_parse_t::nacti(string& s)
{
	formule = s;
}

void LTL_parse_t::nacti(void)
{
#if BUCHI_LANG == BUCHI_LANG_CZ       
	cout << "Zadej LTL formuli> ";
#elif BUCHI_LANG == BUCHI_LANG_EN
	cout << "Enter LTL formula> ";
#endif
	getline(cin,formule);
}
 
void LTL_parse_t::vypis(void)
{
	cout << formule << endl;
}

namespace divine {
istream& operator>>(istream& vstup, LTL_parse_t& a)
{
	getline(vstup,a.formule);
	return(vstup);
}
 
ostream& operator<<(ostream& vystup, LTL_parse_t a)
{
	vystup << a.formule;
	return(vystup);
}
}


void LTL_parse_t::nacti(istream& fr, ostream& vystup)
{
	string s;

	formule = "";

	while (!fr.eof()) {
		getline(fr, s);

		if (s.size() == 0) continue;

		if (s[0] == '#') {
			if ((s.size() > 1) && (s[1] == '@')) {
				vystup << s << endl;
			}
		} else {
			formule = formule + s;
		}
	}
}

void LTL_parse_t::nacti(istream& fr)
{
	string s;

	while (!fr.eof()) {
		getline(fr, s);

		if (s.size() == 0) continue;

		if (s[0] != '#') formule = formule + s;
	}
}

/* BLOK FUNKCI PRO SYNTAKTICKOU ANALYZU */

enum synt_an_el {un_op, bin_op, an_left_par, an_right_par, an_atom,
	an_end, an_unknown, S, A, gnd};
        
struct pol_zasobnik {
	int stav;
	synt_an_el elem;
};
  
struct preved_zas {
	LTL_syntax_el_t elem;
	string pred;   
	int what; // 0 = element; 1 = predikat;
};

LTL_syntax_el_t LTL_parse_t::get_syntax_el(int& i, string& pred)
{
	string pom = "";
	char c;
	int delka;

	delka = formule.length();

	while (((formule[i] == ' ') || (formule[i] == '\t') ||
		(formule[i] == '\n')) && (i < delka)) i++;

	if (i >= delka) return(end);

	switch (formule[i]) {
	case '(': i++; return(left_par); break;
	case ')': i++; return(right_par); break;
	case '!': i++; return(ltl_not); break;
	case 'O':
	case 'X': i++; return(ltl_next); break; 
	case 'U': i++; return(ltl_until); break;
	case 'W': i++; return(ltl_weak); break;
	case 'R':
	case 'V': i++; return(ltl_V); break;
	case '+': i++; return(ltl_or); break;
	case '*': i++; return(ltl_and); break;
	case '^': i++; return(ltl_xor); break;
	case '[': i++; if (formule[i] == ']') {
		i++;
		return(ltl_box);
		} else return(unknown);
		break;
	case 'G': i++; return(ltl_box); break;
	case '<': i++; if (formule[i] == '>') {
		i++;
		return(ltl_diamond);
		} else if (formule[i] == '-') {
			i++;
			if (formule[i] == '>') {
			i++;
			return(ltl_ekv);
			} else return(unknown);
		} else return(unknown);
 		break;
	case 'F': i++; return(ltl_diamond); break;
	case '-': i++; if (formule[i] == '>') {   
		i++;
		return(ltl_impl);
		} else return(unknown);
		break;
	case '|': i++; if (formule[i] == '|') {
		i++;
		return(ltl_or);
		} else return(unknown);
		break;
	case '&': i++; if (formule[i] == '&') {
		i++;
		return(ltl_and);
		} else return(unknown);
		break;
	}

// Muze jit o atom, true nebo false:
	if ( (!islower(formule[i])) && (formule[i] != '_') ) {
		i++;
		return(unknown);
	}
	do {
		pom += formule[i];
		i++;
		c = formule[i];
	} while ((i < delka) &&
		(islower(c) || isdigit(c) || (c == '_') || (c == '=')));

	pred = pom;
	if (pom == "true") return(ltl_true);
	if (pom == "false") return(ltl_false);
	return(ltl_atom);
}

synt_an_el konvert_synt_el(LTL_syntax_el_t e)
{
	switch (e) {
	case ltl_box:
	case ltl_diamond:
	case ltl_not:
	case ltl_next: return(un_op); break;
	case ltl_until:
	case ltl_weak:
	case ltl_V:
	case ltl_and:
	case ltl_or:
	case ltl_xor:
	case ltl_impl:
	case ltl_ekv: return(bin_op); break;
	case right_par: return(an_right_par); break;
 	case left_par: return(an_left_par); break;  
	case ltl_true:
	case ltl_false:
	case ltl_atom: return(an_atom); break;
	case end: return(an_end); break;
	case unknown: return(an_unknown); break;
        default: gerr << "Unexpected value of \"e\"" << thr();
                 return an_unknown; //unreachable
                 break;
	}
}

bool redukuj(stack<pol_zasobnik>& Z, int pravidlo, synt_an_el& N)
{
	pol_zasobnik pom;

	switch (pravidlo) {
	case 0: pom = Z.top(); // S' -> S
		if (pom.elem == S) {
			Z.pop();
			N = gnd; return(true);
		} else return(false);
		break;
	case 1: pom = Z.top(); // S -> un_op A
		if (pom.elem == A) {
			Z.pop(); pom = Z.top();
			if (pom.elem == un_op) {
				Z.pop();
				N = S; return(true);
			} else return(false);
		} else return(false);
		break;
	case 2: pom = Z.top(); // S -> S bin_op A
		if (pom.elem == A) {
			Z.pop(); pom = Z.top();
			if (pom.elem == bin_op) {
				Z.pop(); pom = Z.top();
				if (pom.elem == S) {   
					Z.pop();
					N = S; return(true);
				} else return(false);
			} else return(false);
		} else return(false);
		break;
	case 3: pom = Z.top(); // S -> (S)
		if (pom.elem == an_right_par) {
			Z.pop(); pom = Z.top();
			if (pom.elem == S) {   
				Z.pop(); pom = Z.top();
				if (pom.elem == an_left_par) {
					Z.pop();
					N = S; return(true);
				} else return(false);
			} else return(false);
		} else return(false);
		break;
	case 4: pom = Z.top(); // S -> an_atom
		if (pom.elem == an_atom) {
			Z.pop();
			N = S; return(true);
		} else return(false);
		break;
	case 5: pom = Z.top(); // A -> un_op A
		if (pom.elem == A) {
			Z.pop(); pom = Z.top();
			if (pom.elem == un_op) {
				Z.pop();
				N = A; return(true);
			} else return(false);
		} else return(false);
		break;
	case 6: pom = Z.top(); // A -> (S)
		if (pom.elem == an_right_par) {
			Z.pop(); pom = Z.top();
			if (pom.elem == S) {   
				Z.pop(); pom = Z.top();
				if (pom.elem == an_left_par) {
					Z.pop();
					N = A; return(true);
				} else return(false);
			} else return(false);
		} else return(false);
		break;
	case 7: pom = Z.top(); // A -> an_atom
		if (pom.elem == an_atom) {
			Z.pop();
			N = A; return(true);
		} else return(false);
		break;
	default: return(false);
	}
}

void preved_atom(string& s, LTL_formul_t& F)
{
	if (s == "true") {
		F.add_bool(true);
		return;
	}
	if (s == "false") {
		F.add_bool(false);
		return;
	}
	F.add_literal(s);

	return;
}

void preved_un_op(LTL_syntax_el_t elem, LTL_formul_t& F1,
	LTL_formul_t& F2) // F2 = elem F1
{
	LTL_formul_t Fpom;

	switch (elem) {
	case ltl_not: F2 = F1.negace();
		break;
	case ltl_next: F2.add_next_op(F1);
		break;
	case ltl_diamond: Fpom.add_bool(true);
		F2.add_binar_op(op_U, Fpom, F1);
		break;
	case ltl_box: Fpom.add_bool(false);
		F2.add_binar_op(op_V, Fpom, F1);
		break;
        default: break;//nothing
	}
}

void preved_bin_op(LTL_syntax_el_t elem, LTL_formul_t& F1,
	LTL_formul_t& F2, LTL_formul_t& F3)
{ // F3 = F1 elem F2
	LTL_formul_t Fpom1, Fpom2, Fpom3, Fpom4;

	switch (elem) {
	case ltl_or: F3.add_binar_op(op_or, F1, F2);
		break;
	case ltl_and: F3.add_binar_op(op_and, F1, F2);
		break;
	case ltl_until: F3.add_binar_op(op_U, F1, F2);
		break;
	case ltl_V: F3.add_binar_op(op_V, F1, F2);
		break;
	case ltl_xor: // F1 ^ F2 <=> (F1 && !F2) || (!F1 && F2)
		Fpom1 = F1.negace(); Fpom2 = F2.negace();
		Fpom3.add_binar_op(op_and, F1, Fpom2);
		Fpom4.add_binar_op(op_and, Fpom1, F2);
		F3.add_binar_op(op_or, Fpom3, Fpom4);
		break;
	case ltl_impl: Fpom1 = F1.negace(); // F1->F2 <=> !F1 || F2
		F3.add_binar_op(op_or, Fpom1, F2);
		break;
	case ltl_ekv: // F1<->F2 <=> (F1 && F2) || (!F1 && !F2)
		Fpom1 = F1.negace();
		Fpom2 = F2.negace();
		Fpom3.add_binar_op(op_and, F1, F2);
		Fpom4.add_binar_op(op_and, Fpom1, Fpom2);
		F3.add_binar_op(op_or, Fpom3, Fpom4);
		break;
	case ltl_weak: /* F1 W F2 <=> GF1 || (F1 U F2) <=>
			 (false V F1) || (F1 U F2) */
		/*Fpom1.add_bool(false);
		Fpom2.add_binar_op(op_V, Fpom1, F1);
		Fpom3.add_binar_op(op_U, F1, F2);
		F3.add_binar_op(op_or, Fpom2, Fpom3);
		break;*/
		/* F1 U (F2 || GF1) <=> F1 U (F2 || (false V F1)) */
		Fpom1.add_bool(false);
		Fpom2.add_binar_op(op_V, Fpom1, F1);
		Fpom3.add_binar_op(op_or, F2, Fpom2);
		F3.add_binar_op(op_U, F1, Fpom3);
		break;
        default: break; //nothing
	}
}


bool LTL_parse_t::syntax_check(LTL_formul_t& F)
{
	int i, old_i, delka, stav;
	string pred;
	LTL_syntax_el_t elem;
	synt_an_el an_elem, N;
	pol_zasobnik pom;
	preved_zas prev_pom;
	stack<pol_zasobnik> Z;
	stack<LTL_formul_t> ZF;
	stack<preved_zas> Zprev;
	LTL_formul_t F1, F2, F3;

	delka = formule.length();
	stav = 0; i = 0; old_i = 0; N = gnd;

	pom.stav = 0; pom.elem = gnd; Z.push(pom);

	do {
		if (N == gnd) {
			old_i = i;
			elem = get_syntax_el(i, pred);
			an_elem = konvert_synt_el(elem);
		}

		switch (stav) {
		case 0: if (N == S) {
				pom.stav = 1; pom.elem = S;
				Z.push(pom);
				N = gnd;
				stav = 1; break;
			}
			if ((N == gnd) && (an_elem == un_op)) {
				pom.elem = an_elem; pom.stav = 2;
				Z.push(pom);
				stav = 2;   

				prev_pom.what = 0;
				prev_pom.elem = elem;
				Zprev.push(prev_pom);
				break;
			}
			if ((N == gnd) && (an_elem == an_atom)) {
				pom.elem = an_elem; pom.stav = 4;
				Z.push(pom);
				stav = 4;

				prev_pom.what = 1;
				prev_pom.pred = pred;
				Zprev.push(prev_pom);
				break;
			}
			if ((N == gnd) && (an_elem == an_left_par)) {
				pom.elem = an_elem; pom.stav = 3;
				Z.push(pom);
				stav = 3; break;
			}
			stav = -1; break;
		case 1: if ((N == gnd) && (an_elem == an_end) ) {
				i = old_i;
				if (redukuj(Z,0,N)) {
					pom = Z.top();
					if (pom.elem == gnd) {
						F = ZF.top(); ZF.pop();
						return(true);
					}
				}
			}
			if ((N == gnd) && (an_elem == bin_op)) {
				pom.elem = an_elem; pom.stav = 5;
				Z.push(pom);
				stav = 5;

				prev_pom.what = 0;
				prev_pom.elem = elem;
				Zprev.push(prev_pom);
				break;
			}
			stav = -1; break;
		case 2: if (N == A) {
				pom.stav = 6; pom.elem = A;
				Z.push(pom);
				N = gnd;
				stav = 6; break;
			}
			if ((N == gnd) && (an_elem == un_op)) {
				pom.stav = 7; pom.elem = an_elem;
				Z.push(pom);
				stav = 7;

				prev_pom.what = 0;
				prev_pom.elem = elem;
				Zprev.push(prev_pom);
				break;
			}
			if ((N == gnd) && (an_elem == an_atom)) {
				pom.stav = 9; pom.elem = an_elem;
				Z.push(pom);
				stav = 9;

				prev_pom.what = 1;
				prev_pom.pred = pred;
				Zprev.push(prev_pom);
				break;
			}
			if ((N == gnd) && (an_elem == an_left_par)) {
				pom.stav = 8; pom.elem = an_elem;   
				Z.push(pom);
				stav = 8; break;
			}
			stav = -1; break;
		case 3: if (N == S) {
				pom.stav = 10; pom.elem = S;
				Z.push(pom);
				N = gnd;
				stav = 10; break;
			}
			if ((N == gnd) && (an_elem == un_op)) {
				pom.stav = 2; pom.elem = an_elem;
				Z.push(pom);
				stav = 2;

				prev_pom.what = 0;
				prev_pom.elem = elem;
				Zprev.push(prev_pom);
				break;
			}
			if ((N == gnd) && (an_elem == an_atom)) {
				pom.stav = 4; pom.elem = an_elem;
				Z.push(pom);
				stav = 4;

				prev_pom.what = 1;
				prev_pom.pred = pred;
				Zprev.push(prev_pom);
				break;
			}
			if ((N == gnd) && (an_elem == an_left_par)) {
				pom.stav = 3; pom.elem = an_elem;
				Z.push(pom);
				stav = 3; break;
			}
			stav = -1; break;
		case 4: if ((an_elem == an_right_par) ||
			(an_elem == an_end) || (an_elem == bin_op)) {
				i = old_i;
				if (redukuj(Z, 4, N)) {
					pom = Z.top();
					stav = pom.stav;

					prev_pom = Zprev.top();
					Zprev.pop();
					preved_atom(prev_pom.pred, F1);
					ZF.push(F1);
				} else stav = -1;
				break;
			}
			stav = -1; break;
		case 5: if (N == A) {
				pom.stav = 11; pom.elem = A;
				Z.push(pom);
				N = gnd;
				stav = 11; break;
			}
			if ((N == gnd) && (an_elem == un_op)) {
				pom.stav = 7; pom.elem = an_elem;
				Z.push(pom);
				stav = 7;

				prev_pom.what = 0;
				prev_pom.elem = elem;
				Zprev.push(prev_pom);
				break;
			}
			if ((N == gnd) && (an_elem == an_atom)) {
				pom.stav = 9; pom.elem = an_elem;
				Z.push(pom);
				stav = 9;

				prev_pom.what = 1;
				prev_pom.pred = pred;
				Zprev.push(prev_pom);
				break;
			}
			if ((N == gnd) && (an_elem == an_left_par)) {
				pom.stav = 8; pom.elem = an_elem;
				Z.push(pom);
				stav = 8; break;
			}
			stav = -1; break;
		case 6: if ((an_elem == an_right_par) ||
			(an_elem == an_end) || (an_elem == bin_op)) {
				i = old_i;
				if (redukuj(Z,1,N)) {
					pom = Z.top();
					stav = pom.stav;

					F1.clear(); F2.clear();
					F1 = ZF.top(); ZF.pop();
					prev_pom = Zprev.top(); 
					Zprev.pop();
					preved_un_op(prev_pom.elem,F1,F2);
					ZF.push(F2);
				} else stav = -1;   
				break;
			}
 			stav = -1; break;
		case 7: if (N == A) {
				pom.stav = 12; pom.elem = A;
				Z.push(pom);
				N = gnd;
				stav = 12; break;
			}
			if ((N == gnd) && (an_elem == un_op)) {
				pom.elem = an_elem; pom.stav = 7;
				Z.push(pom);
				stav = 7;

				prev_pom.what = 0;
				prev_pom.elem = elem;
				Zprev.push(prev_pom);
				break;
			}
			if ((N == gnd) && (an_elem == an_atom)) {
				pom.elem = an_elem; pom.stav = 9;
				Z.push(pom);
				stav = 9;

				prev_pom.what = 1;
				prev_pom.pred = pred;
				Zprev.push(prev_pom);
				break;
			}
			if ((N == gnd) && (an_elem == an_left_par)) {
				pom.elem = an_elem; pom.stav = 8;
				Z.push(pom);
				stav = 8; break;
			}
			stav = -1; break;
		case 8: if (N == S) {
				pom.stav = 13; pom.elem = S;
				Z.push(pom);
				N = gnd;
				stav = 13; break;
			}
			if ((N == gnd) && (an_elem == un_op)) {
				pom.elem = an_elem; pom.stav = 2;
				Z.push(pom);
				stav = 2;   

				prev_pom.what = 0;
				prev_pom.elem = elem;
				Zprev.push(prev_pom);
				break;
			}
			if ((N == gnd) && (an_elem == an_atom)) {
				pom.elem = an_elem; pom.stav = 4;
				Z.push(pom);
				stav = 4;   

				prev_pom.what = 1;
				prev_pom.pred = pred;
				Zprev.push(prev_pom);
				break;
			}
			if ((N == gnd) && (an_elem == an_left_par)) {
				pom.elem = an_elem; pom.stav = 3;
				Z.push(pom);
				stav = 3; break;
			}
			stav = -1; break;
		case 9: if ((an_elem == an_end) || (an_elem == bin_op) ||
				(an_elem == an_right_par) ) {
				i = old_i;
				if (redukuj(Z,7,N)) {
					pom = Z.top();
					stav = pom.stav;

					prev_pom = Zprev.top();
					Zprev.pop();
					preved_atom(prev_pom.pred, F1);
					ZF.push(F1);
				} else stav = -1;
				break;
			}
			stav = -1; break;
		case 10: if ((N == gnd) && (an_elem == bin_op)) {
				pom.stav = 5; pom.elem = an_elem;
				Z.push(pom);
				stav = 5;

				prev_pom.what = 0;
				prev_pom.elem = elem;
				Zprev.push(prev_pom);
				break;
			}
			if ((N == gnd) && (an_elem == an_right_par)) {
				pom.stav = 14; pom.elem = an_elem;
				Z.push(pom);
				stav = 14;
				break;
			}
			stav = -1; break;
		case 11: if ((an_elem == an_right_par) ||
			(an_elem == bin_op) || (an_elem == an_end)) {
				i = old_i;
				if (redukuj(Z, 2, N)) {
					pom = Z.top();
					stav = pom.stav;

					prev_pom = Zprev.top();
					Zprev.pop();
					F1.clear(); F2.clear();
					F3.clear();
					F1 = ZF.top(); ZF.pop();
					F2 = ZF.top(); ZF.pop();
				    preved_bin_op(prev_pom.elem,F2,F1,F3);
					ZF.push(F3);
				} else stav = -1;
				break;
			}
			stav = -1; break;
		case 12: if ((an_elem == an_right_par) ||
			(an_elem == bin_op) || (an_elem == an_end) ) {
				i = old_i;
				if (redukuj(Z,5,N)) {
					pom = Z.top();
					stav = pom.stav;

					F1.clear(); F2.clear();
					F1 = ZF.top(); ZF.pop();
					prev_pom = Zprev.top();
					Zprev.pop();
					preved_un_op(prev_pom.elem,F1,F2);
					ZF.push(F2);
				} else stav = -1;
				break;
			}
			stav = -1; break;
		case 13: if ((N == gnd) && (an_elem == bin_op)) {
				pom.stav = 5; pom.elem = an_elem;
				Z.push(pom);
				stav = 5;

				prev_pom.what = 0;
				prev_pom.elem = elem;
				Zprev.push(prev_pom);
				break;
			} 
			if ((N == gnd) && (an_elem == an_right_par)) {
				pom.elem = an_elem; pom.stav = 15;
				Z.push(pom);
				stav = 15; break;
			}
			stav = -1; break;
		case 14: if ((an_elem == an_right_par) ||
			(an_elem == an_end) || (an_elem == bin_op)) {
				i = old_i;
				if (redukuj(Z, 3, N)) {
					pom = Z.top();
					stav = pom.stav;
				} else stav = -1;
				break;
			}
			stav = -1; break;
		case 15: if ((an_elem == an_right_par) ||
			(an_elem == an_end) || (an_elem == bin_op)) {
				i = old_i;
				if (redukuj(Z, 6, N)) {
					pom = Z.top();
					stav = pom.stav;
				} else stav = -1;
				break;
			}
			stav = -1; break;
		}

	} while (stav != -1);

	return(false);
}

/* KONEC BLOKU SYNTAKTICKA ANALYZA */

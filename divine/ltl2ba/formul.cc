/*
*  representation of LTL formula - implementation
* 
*  Milan Jaros - xjaros1@fi.muni.cz
* 
*/

#include <iostream>
#include <string>
#include <stack>
#include <iterator>
#include <algorithm>
#include "formul.hh"

#include <sevine.h>

// #include "error/error.hh"
// #include "deb/deb.hh"

#ifndef DOXYGEN_PROCESSING

using namespace divine;
using namespace std;

#endif //DOXYGEN_PROCESSING

struct ukaz_zas {
	LTL_formul_t *z1, *z2;
};

ostream& divine::operator<<(ostream& vystup, const LTL_literal_t L)
{
	if (!L.negace) vystup << LTL_OUT_NOT;
	return(vystup << L.predikat);
}

void LTL_formul_t::kopiruj(const LTL_formul_t *a)
{
	const LTL_formul_t *f1;
	LTL_formul_t *f2;
	ukaz_zas pom;
	stack<ukaz_zas> Z;
	bool pokrac = true;

	f1 = a;
	f2 = this;

	while (pokrac) {
		f2->what = f1->what;
		switch (f1->what) {
		case LTL_LITERAL:
			f2->obsah.o2 = new LTL_literal_t;
			f2->obsah.o2->negace = f1->obsah.o2->negace;
			f2->obsah.o2->predikat = f1->obsah.o2->predikat;
			break;
		case LTL_BIN_OP:
			f2->obsah.o1 = new LTL_binar_op_t;
			f2->obsah.o1->op = f1->obsah.o1->op;
			f2->obsah.o1->arg1 = new LTL_formul_t;
			f2->obsah.o1->arg2 = new LTL_formul_t;
			pom.z1 = f1->obsah.o1->arg1;
			pom.z2 = f2->obsah.o1->arg1;
			Z.push(pom);
			pom.z1 = f1->obsah.o1->arg2;
			pom.z2 = f2->obsah.o1->arg2;
			Z.push(pom);
			break;
		case LTL_BOOL:
			f2->obsah.o3 = f1->obsah.o3;
			break;
		case LTL_NEXT_OP:
			f2->obsah.o4 = new LTL_formul_t;
			pom.z1 = f1->obsah.o4;
			pom.z2 = f2->obsah.o4;
			Z.push(pom);
			break;
		}

		if (Z.empty()) {
			pokrac = false;
		} else {
			pom = Z.top(); Z.pop();
			f1 = pom.z1; f2 = pom.z2;
		}
	}
}

bool LTL_formul_t::srovnej(const LTL_formul_t& a) const
{
	const LTL_formul_t *f1, *f2;
	ukaz_zas pom;
	stack<ukaz_zas> Z;
	bool pokrac = true, same = true;
                                 
	f1 = &a;
	f2 = this;
	while (pokrac) {
		if (f1->what == f2->what) {
			switch (f1->what) {
			case LTL_LITERAL:
				if (f1->obsah.o2->negace !=
				f2->obsah.o2->negace) {
					pokrac = false;
					same = false;
				}
				if (f1->obsah.o2->predikat !=
				f2->obsah.o2->predikat) {
					pokrac = false;
					same = false;
				}
				break;
			case LTL_BIN_OP:
				if (f1->obsah.o1->op !=
				f2->obsah.o1->op) {
					pokrac = false;
					same = false;
				} else {
					pom.z1 = f1->obsah.o1->arg1;
					pom.z2 = f2->obsah.o1->arg1;
					Z.push(pom);
					pom.z1 = f1->obsah.o1->arg2;
					pom.z2 = f2->obsah.o1->arg2;
					Z.push(pom);
				}
				break;
			case LTL_BOOL:
				if (f1->obsah.o3 != f2->obsah.o3) {
					pokrac = false;
					same = false;
				}
				break;
			case LTL_NEXT_OP:
				pom.z1 = f1->obsah.o4;
				pom.z2 = f2->obsah.o4;
				Z.push(pom);
				break;
			}
		} else {
			pokrac = false;
			same = false;
		}

		if (Z.empty()) {
			pokrac = false;
		} else {
			pom = Z.top(); Z.pop();
			f1 = pom.z1; f2 = pom.z2;
		}
	}

	return(same);
}

void LTL_formul_t::uvolni()
{
	switch (what) {
	case LTL_LITERAL:
		delete obsah.o2;
		break;
	case LTL_BIN_OP:
		delete obsah.o1->arg1;
		delete obsah.o1->arg2;
		delete obsah.o1;
		break;
	case LTL_NEXT_OP:
		delete obsah.o4;
		break;
	}
}

ostream& LTL_formul_t::vypis(ostream& vystup)
{
	switch (what) {
	case LTL_LITERAL:
		if (! obsah.o2->negace) vystup << LTL_OUT_NOT;
		vystup << obsah.o2->predikat;
		break;
	case LTL_BIN_OP:
		vystup << "(";
		obsah.o1->arg1->vypis(vystup);
		switch (obsah.o1->op) {
		case op_and: vystup << LTL_OUT_AND; break;
		case op_or: vystup << LTL_OUT_OR; break;
		case op_U: vystup << LTL_OUT_U; break;
		case op_V: vystup << LTL_OUT_V; break;
		}
		obsah.o1->arg2->vypis(vystup);
		vystup << ")";
		break;
	case LTL_BOOL:
		if (obsah.o3)
			vystup << "true";
		else
			vystup << "false";
		break;
	case LTL_NEXT_OP:
		vystup << LTL_OUT_X;
		obsah.o4->vypis(vystup);
		break;
	}

	return vystup;
}

LTL_formul_t::LTL_formul_t():what(LTL_EMPTY)
{ }

LTL_formul_t::LTL_formul_t(const LTL_formul_t& a)
{
	kopiruj(&a);
}

LTL_formul_t::~LTL_formul_t()
{
	if (what != LTL_EMPTY) uvolni();
}

void LTL_formul_t::clear()
{
	if (what != LTL_EMPTY) uvolni();
	what = LTL_EMPTY;
}

void LTL_formul_t::add_literal(string pred)
{
	if (what != LTL_EMPTY) uvolni();

	what = LTL_LITERAL;
	obsah.o2 = new LTL_literal_t;
	obsah.o2->negace = true; // nenegovany literal
	obsah.o2->predikat = pred;
}

void LTL_formul_t::add_literal(bool neg, string pred)
{
	if (what != LTL_EMPTY) uvolni();

	what = LTL_LITERAL;
	obsah.o2 = new LTL_literal_t;
	obsah.o2->negace = neg;
	obsah.o2->predikat = pred;
}

void LTL_formul_t::add_binar_op(LTL_oper_t op, LTL_formul_t& arg1,
	LTL_formul_t& arg2)
{
	if (what != LTL_EMPTY) uvolni();

	what = LTL_BIN_OP;
	obsah.o1 = new LTL_binar_op_t;
	obsah.o1->op = op;
	obsah.o1->arg1 = new LTL_formul_t(arg1);
	obsah.o1->arg2 = new LTL_formul_t(arg2);
}

void LTL_formul_t::add_bool(bool b)
{
	if (what != LTL_EMPTY) uvolni();

	what = LTL_BOOL;
	obsah.o3 = b;
}

void LTL_formul_t::add_next_op(LTL_formul_t& arg)
{
	if (what != LTL_EMPTY) uvolni();

	what = LTL_NEXT_OP;
	obsah.o4 = new LTL_formul_t(arg);
}

int LTL_formul_t::typ() const
{
	return(what);
}

bool LTL_formul_t::get_literal(bool& neg, string& pred) const
{
	if (what == LTL_LITERAL) {
		neg = obsah.o2->negace;
		pred = obsah.o2->predikat;
		return(true);
	} else return(false);
}

bool LTL_formul_t::get_binar_op(LTL_oper_t& op, LTL_formul_t& arg1,
	LTL_formul_t& arg2) const
{
	if (what == LTL_BIN_OP) {
	        arg1.clear();
	        arg2.clear();
		op = obsah.o1->op;
		arg1.kopiruj(obsah.o1->arg1);
		arg2.kopiruj(obsah.o1->arg2);
		return(true);
	} else return(false);
}

bool LTL_formul_t::get_binar_op(LTL_oper_t& op) const
{
	if (what == LTL_BIN_OP) {
		op = obsah.o1->op;
		return(true);
	} else return(false);
}

bool LTL_formul_t::get_bool(bool& b) const
{
	if (what == LTL_BOOL) {
		b = obsah.o3;
		return(true);
	} else return(false);
}

bool LTL_formul_t::get_next_op(LTL_formul_t& arg) const
{
	if (what == LTL_NEXT_OP) {
		arg.kopiruj(obsah.o4);
		return(true);
	} else return(false);
}

LTL_formul_t& LTL_formul_t::negace()
{
	LTL_formul_t *fv, *f1, *FV;
	stack<ukaz_zas> Z;
	ukaz_zas pom;
	bool pokrac;

	FV = new LTL_formul_t;
	fv = FV; f1 = this;
	pokrac = true;

	while (pokrac) {
		fv->what = f1->what;
		switch (f1->what) {
		case LTL_LITERAL:
			fv->obsah.o2 = new LTL_literal_t;
			fv->obsah.o2->negace = !(f1->obsah.o2->negace);
			fv->obsah.o2->predikat = f1->obsah.o2->predikat;
			break;
		case LTL_BOOL:
			fv->obsah.o3 = !(f1->obsah.o3);
			break;
		case LTL_NEXT_OP:
			fv->obsah.o4 = new LTL_formul_t;
			pom.z1 = f1->obsah.o4;
			pom.z2 = fv->obsah.o4;
			Z.push(pom);
			break;
		case LTL_BIN_OP: fv->obsah.o1 = new LTL_binar_op_t;
			fv->obsah.o1->arg1 = new LTL_formul_t;
			fv->obsah.o1->arg2 = new LTL_formul_t;
			pom.z1 = f1->obsah.o1->arg1;
			pom.z2 = fv->obsah.o1->arg1;
			Z.push(pom);
			pom.z1 = f1->obsah.o1->arg2;
			pom.z2 = fv->obsah.o1->arg2;
			Z.push(pom);
			switch (f1->obsah.o1->op) {
			case op_and: fv->obsah.o1->op = op_or;
				break;
			case op_or: fv->obsah.o1->op = op_and;
				break;
			case op_U: fv->obsah.o1->op = op_V;
				break;
			case op_V: fv->obsah.o1->op = op_U;
				break;
			}
			break;
		}

		if (Z.empty()) {
			pokrac = false;
		} else {
			pom = Z.top(); Z.pop();
			f1 = pom.z1;
			fv = pom.z2;
		}
	}

	return(*FV);
}

/* Funkce pro prepsani LTL_formul_te na jednodussi */

bool LTL_formul_t::eql(LTL_formul_t& F1, LTL_formul_t& F2)
{
	LTL_formul_t Fp1, Fp2;
	LTL_oper_t op;

	if (F1 == F2) {
		//cerr << F1 << " = " << F2 << endl;
		return(true);
	}
	if (F2.what == LTL_BOOL)
		if (F2.obsah.o3) return(true);
	if (F1.what == LTL_BOOL)
		if (!(F1.obsah.o3)) return(true);

	if (F2.get_binar_op(op, Fp1, Fp2)) {
		switch (op) {
		case op_and:
		//cerr << "porovnani 1.1 " << F1 << " " << F2 << endl;
			return(eql(F1, Fp1) && eql(F1, Fp2));
		case op_or:
		//cerr << "porovnani 1.2 " << F1 << " " << F2 << endl;
			//return(eql(F1, Fp1) || eql(F1, Fp2));
			return(eql(F1, Fp1) && eql(F1, Fp2));
		case op_U:
		//cerr << "porovnani 1.3 " << F1 << " " << F2 << endl;
			return(eql(F1, Fp2));
		case op_V:
		//cerr << "porovnani 1.4 " << F1 << " " << F2 << endl;
			return(eql(F1, Fp1) && eql(F1, Fp2));
		}
	}
	Fp1.clear(); Fp2.clear();

	if (F1.get_binar_op(op, Fp1, Fp2)) {
		switch (op) {
		case op_and:
		//cerr << "porovnani 2.1 " << F1 << " " << F2 << endl;
			//return(eql(Fp1, F2) || eql(Fp2, F2));//puvodni
			return(eql(Fp1, F2) && eql(Fp2, F2));
		case op_or:
		//cerr << "porovnani 2.2 " << F1 << " " << F2 << endl;
			return(eql(Fp1, F2) && eql(Fp2, F2));
			//return(eql(Fp1, F2) || eql(Fp2, F2)); // ma byt?
		case op_U:
		//cerr << "porovnani 2.3 " << F1 << " " << F2 << endl;
			return(eql(Fp1, F2) && eql(Fp2, F2));
		case op_V:
		//cerr << "porovnani 2.4 " << F1 << " " << F2 << endl;
			return(eql(Fp2, F2));
		}
	}

	//cerr << F1 << " !(<=) " << F2 << endl;
	return(false);
}

bool LTL_formul_t::is_GF()
{
	LTL_formul_t *fp1, *fp2;

	if (what == LTL_BIN_OP) {
		if (obsah.o1->op == op_V) {
			fp1 = obsah.o1->arg1;
			fp2 = obsah.o1->arg2;
			if ((fp1->what == LTL_BOOL) &&
				(fp2->what == LTL_BIN_OP)) {
			if (!(fp1->obsah.o3) &&
				(fp2->obsah.o1->op == op_U)) {
				fp1 = fp2->obsah.o1->arg1;
				if (fp1->what == LTL_BOOL) {
					if (fp1->obsah.o3)
						return(true);
				}
			}
			}
		}
	}

	return(false);
}

bool LTL_formul_t::is_GF(LTL_formul_t& F1)
{
	LTL_formul_t *fp1, *fp2;

	if (what == LTL_BIN_OP) {
		if (obsah.o1->op == op_V) {
			fp1 = obsah.o1->arg1;
			fp2 = obsah.o1->arg2;
			if ((fp1->what == LTL_BOOL) &&
				(fp2->what == LTL_BIN_OP)) {
			if (!(fp1->obsah.o3) &&
				(fp2->obsah.o1->op == op_U)) {
				fp1 = fp2->obsah.o1->arg1;
				if (fp1->what == LTL_BOOL) {
					if (fp1->obsah.o3) {
						F1=*(fp2->obsah.o1->arg2);
						return(true);
					}
				}
			}
			}
		}
	}

	return(false);
}

bool LTL_formul_t::is_FG()
{
	LTL_formul_t *fp1, *fp2;

	if (what == LTL_BIN_OP) {
		if (obsah.o1->op == op_U) {
			fp1 = obsah.o1->arg1;
			fp2 = obsah.o1->arg2;
			if ((fp1->what == LTL_BOOL) &&
				(fp2->what == LTL_BIN_OP)) {
			if (fp1->obsah.o3 &&
				(fp2->obsah.o1->op == op_V)) {
				fp1 = fp2->obsah.o1->arg1;
				if (fp1->what == LTL_BOOL) {
					if (!(fp1->obsah.o3))
						return(true);
				}
			}
			}
		}
	}

	return(false);
}

bool LTL_formul_t::is_FG(LTL_formul_t& F1)
{
	LTL_formul_t *fp1, *fp2;

	if (what == LTL_BIN_OP) {
		if (obsah.o1->op == op_U) {
			fp1 = obsah.o1->arg1;
			fp2 = obsah.o1->arg2;
			if ((fp1->what == LTL_BOOL) &&
				(fp2->what == LTL_BIN_OP)) {
			if (fp1->obsah.o3 &&
				(fp2->obsah.o1->op == op_V)) {
				fp1 = fp2->obsah.o1->arg1;
				if (fp1->what == LTL_BOOL) {
					if (!(fp1->obsah.o3)) {
						F1=*(fp2->obsah.o1->arg2);
						return(true);
					}
				}
			} 
			}
		}
	}

	return(false);
}

//#define REWRITE_DEB

void LTL_formul_t::rewrite_and(LTL_formul_t *F1, LTL_formul_t *F2,
	LTL_formul_t& FV)
{
	LTL_formul_t FV1, FV2, F_neg, Fp1, Fp2, Fp3, Fp4, Fpom;
	LTL_oper_t op1, op2;

	#ifdef REWRITE_DEB
	cerr << "rewrite " <<  (*F1) << " AND " << (*F2) << endl;
	#endif

	F1->rewrite(FV1);
	F2->rewrite(FV2);

	if (eql(FV1, FV2)) { // FV1 < FV2 => (FV1 && FV2) = FV1
		FV = FV1; return;
	}
	if (eql(FV2, FV1)) {
		FV = FV2; return;
	}

	F_neg = FV2.negace(); // FV1 < !FV2 => (FV1 && FV2) = false
	if (eql(FV1, F_neg)) {
		FV.add_bool(false); return;
	}
	F_neg.clear(); F_neg = FV1.negace();
	if (eql(FV2, F_neg)) {
		FV.add_bool(false); return;
	}

	// FG Fp1 && FG Fp2 = FG(Fp1 && Fp2)
	if (FV1.is_FG(Fp1) && FV2.is_FG(Fp2)) {
		Fp3.add_binar_op(op_and, Fp1, Fp2);
		Fp1.clear(); Fp1.add_bool(false);
		Fp2.clear(); Fp2.add_binar_op(op_V, Fp1, Fp3);
		Fp1.clear(); Fp1.add_bool(true);
		Fp4.add_binar_op(op_U, Fp1, Fp2);
		Fp4.rewrite(FV);
		return;
	}
	Fp1.clear(); Fp2.clear(); Fp3.clear(); Fp4.clear();

	// (Fp1 R Fp2) && (Fp1 R Fp4) = Fp1 R (Fp2 && Fp4)
	if (FV1.get_binar_op(op1, Fp1, Fp2) &&
		FV2.get_binar_op(op2, Fp3, Fp4)) {
		if ((op1 == op_V) && (op2 == op_V)) {
			if (Fp1 == Fp3) {
				Fpom.add_binar_op(op_and, Fp2, Fp4);
				rewrite_V(&Fp1, &Fpom, FV);
				return;
			}
		}
	}
	Fp1.clear(); Fp2.clear(); Fp3.clear(); Fp4.clear();

	// (Fp1 U Fp2) && (Fp3 U Fp2) = (Fp1 && Fp3) U Fp2
        if (FV1.get_binar_op(op1, Fp1, Fp2) &&
                FV2.get_binar_op(op2, Fp3, Fp4)) {
                if ((op1 == op_U) && (op2 == op_U)) {
                        if (Fp2 == Fp4) {
                                Fpom.add_binar_op(op_and, Fp1, Fp3);
                                rewrite_U(&Fpom, &Fp2, FV);
                                return;
                        }
                }
        }
	Fp1.clear(); Fp2.clear(); Fp3.clear(); Fp4.clear();

	// XFp1 && XFp2 = X(Fp1 && Fp2)
	if (FV1.get_next_op(Fp1) && FV2.get_next_op(Fp2)) {
		Fpom.add_binar_op(op_and, Fp1, Fp2);
		rewrite_X(&Fpom, FV);
		return;
	}


	FV.add_binar_op(op_and, FV1, FV2);
}

void LTL_formul_t::rewrite_or(LTL_formul_t *F1, LTL_formul_t *F2,
	LTL_formul_t& FV)
{
	LTL_formul_t F_neg, FV1, FV2, Fp1, Fp2, Fp3, Fp4, Fpom;
	LTL_oper_t op1, op2;

	#ifdef REWRITE_DEB
	cerr << "rewrite " <<  (*F1) << " OR " << (*F2) << endl;
	#endif

	F1->rewrite(FV1);
	F2->rewrite(FV2);

	if (eql(FV1, FV2)) { // F1 < F2 => (F1 || F2) = F2
		#ifdef REWRITE_DEB
		cerr << "pravidlo 1.1" << endl;
		#endif
		FV = FV2; return;
	}
	if (eql(FV2, FV1)) {
		#ifdef REWRITE_DEB
		cerr << "pravidlo 1.2" << endl;
		#endif
		FV = FV1; return;
	}

	/*F_neg = FV2.negace();
	if (eql(FV1, F_neg)) { // F1 < !F2 => (F1 || F2) = true
		#ifdef REWRITE_DEB
		cerr << "pravidlo 2.1 " << FV1 << " " << F_neg << endl;
		#endif
		FV.add_bool(true); return;
	}
	F_neg.clear(); F_neg = FV1.negace();
	if (eql(FV2, F_neg)) {
		#ifdef REWRITE_DEB
		cerr << "pravidlo 2.2" << FV2 << " " << F_neg << endl;
		#endif
		FV.add_bool(true); return;
	}*/

	// (Fp1 R Fp2) || (Fp3 R Fp2) = (Fp1 || Fp3) R Fp2
	if (FV1.get_binar_op(op1, Fp1, Fp2) &&
		FV2.get_binar_op(op2, Fp3, Fp4)) {
		if ((op1 == op_V) && (op2 == op_V)) {
			if (Fp2 == Fp4) {
				Fpom.add_binar_op(op_or, Fp1, Fp3);
				rewrite_V(&Fpom, &Fp2, FV);
				return;
			}
		}
	}
	Fp1.clear(); Fp2.clear(); Fp3.clear(); Fp4.clear();

	// (Fp1 U Fp2) || (Fp1 U Fp4) = Fp1 U (Fp2 || Fp4)
        if (FV1.get_binar_op(op1, Fp1, Fp2) &&
                FV2.get_binar_op(op2, Fp3, Fp4)) {
                if ((op1 == op_U) && (op2 == op_U)) {
                        if (Fp1 == Fp3) {
                                Fpom.add_binar_op(op_or, Fp2, Fp4);
                                rewrite_U(&Fp1, &Fpom, FV);
                                return;
                        }
                }
        }
        Fp1.clear(); Fp2.clear(); Fp3.clear(); Fp4.clear();

	// XFp1 || XFp2 = X(Fp1 || Fp2)
	if (FV1.get_next_op(Fp1) && FV2.get_next_op(Fp2)) {
		Fpom.add_binar_op(op_or, Fp1, Fp2);
		rewrite_X(&Fpom, FV);
		return;
	}

	// GF Fp1 || GF Fp2 = GF(Fp1 || Fp2)
	if (FV1.is_GF(Fp1) && FV2.is_GF(Fp2)) {
		Fp3.add_binar_op(op_or, Fp1, Fp2);
		Fp1.clear(); Fp1.add_bool(true);
		Fp2.clear(); Fp2.add_binar_op(op_U, Fp1, Fp3);
		Fp1.clear(); Fp1.add_bool(false);
		Fp4.add_binar_op(op_V, Fp1, Fp2);
		Fp4.rewrite(FV);
		return;
	}

	FV.add_binar_op(op_or, FV1, FV2);
}

void LTL_formul_t::rewrite_U(LTL_formul_t *F1, LTL_formul_t *F2,
	LTL_formul_t& FV)
{
	LTL_formul_t FV1, FV2, F_pom, Fp1, Fp2, Fp3, Fp4;
	LTL_oper_t op;

	F1->rewrite(FV1);
	F2->rewrite(FV2);

	if (FV2.what == LTL_BOOL) { // F1 U b = b
		FV = FV2; return;
	}

	if (eql(FV1, FV2)) { // FV1 < FV2 => FV1 U FV2 = FV2
		FV = FV2; return;
	}

	// FV1 < Fp1 => FV1 U (Fp1 U Fp2) = (Fp1 U Fp2)
	if (FV2.get_binar_op(op, Fp1, Fp2)) {
		if (op == op_U) {
			if (eql(FV1,Fp1)) {
				FV = FV2;
				return;
			}
		}
		Fp1.clear(); Fp2.clear();
	}

	// FV1 U GF Fp = GF Fp
	if (FV2.is_GF()) {
		FV = FV2;
		return;
	}

	// FV1 U FG Fp = FG Fp
	if (FV2.is_FG()) {
		FV = FV2;
		return;
	}

	// F(Fp1 && GF Fp2) = F Fp1 && GF Fp2
	if ((FV1.what == LTL_BOOL) && (FV2.what == LTL_BIN_OP)) {
		if (FV1.obsah.o3 && (FV2.obsah.o1->op == op_and)) {
			FV2.get_binar_op(op, Fp1, Fp2);
			if (Fp2.is_GF()) {
				Fp3.add_bool(true);
				Fp4.add_binar_op(op_U, Fp3, Fp1);
				rewrite_and(&Fp4, &Fp2, FV);
				return;
			}
			if (Fp1.is_GF()) {
				Fp3.add_bool(true);
				Fp4.add_binar_op(op_U, Fp3, Fp2);
				rewrite_and(&Fp1, &Fp4, FV);
				return;
			}
			// F(Fp1 && FG Fp2) = F Fp1 && FG Fp2
			if (Fp2.is_FG()) {
				Fp3.add_bool(true);
				Fp4.add_binar_op(op_U, Fp3, Fp1);
				rewrite_and(&Fp4, &Fp2, FV);
				return;
			}
			if (Fp1.is_FG()) {
				Fp3.add_bool(true);
				Fp4.add_binar_op(op_U, Fp3, Fp2);
				rewrite_and(&Fp1, &Fp4, FV);
				return;
			}
			Fp1.clear(); Fp2.clear();
		}
	}

	// FX Fp1 = XF Fp1
	if ((FV1.what == LTL_BOOL) && (FV2.what == LTL_NEXT_OP)) {
		if (FV1.obsah.o3) {
			FV2.get_next_op(Fp1);
			Fp2.add_bool(true);
			Fp3.add_binar_op(op_U, Fp2, Fp1);
			rewrite_X(&Fp3, FV);
			return;
		}
	}

	// !FV2 < FV1 => (FV1 U FV2) = true U FV2
	if (eql(FV2.negace(), FV1)) {
		F_pom.add_bool(true);
		FV.add_binar_op(op_U, F_pom, FV2);
		return;
	}

	// XFp1 U XFp2 = X(Fp1 U Fp2)
	if (FV1.get_next_op(Fp1) && FV2.get_next_op(Fp2)) {
		F_pom.add_binar_op(op_U, Fp1, Fp2);
		rewrite_X(&F_pom, FV);
		return;
	}

	FV.add_binar_op(op_U, FV1, FV2);
}

void LTL_formul_t::rewrite_V(LTL_formul_t *F1, LTL_formul_t *F2,
	LTL_formul_t& FV)
{
	LTL_formul_t FV1, FV2, F_pom, Fp1, Fp2, Fp3, Fp4;
	LTL_oper_t op;

	F1->rewrite(FV1);
	F2->rewrite(FV2);

	if (FV2.what == LTL_BOOL) { // FV1 R b = b
		FV = FV2; return;
	}

	if (eql(FV2, FV1)) { // FV2 < FV1 => (FV1 R FV2) = FV2
		FV = FV2; return;
	}

	// Fp1 < FV1 => FV1 R (Fp1 R Fp2) = Fp1 R Fp2
	if (FV2.get_binar_op(op, Fp1, Fp2)) {
		if (op == op_V) {
			if (eql(Fp1, FV1)) {
				FV = FV2;
				return;
			}
		}
		Fp1.clear(); Fp2.clear();
	}

	// FV1 R GF Fp = GF Fp
	if (FV2.is_GF()) {
		FV = FV2;
		return;
	}

	// FV1 R FG Fp = FG Fp
	if (FV2.is_FG()) {
		FV = FV2;
		return;
	}

	// G(Fp1 || GF Fp2) = G Fp1 || GF Fp2
	if ((FV1.what == LTL_BOOL) && (FV2.what == LTL_BIN_OP)) {  
		if (!(FV1.obsah.o3) && (FV2.obsah.o1->op == op_or)) {
			FV2.get_binar_op(op, Fp1, Fp2);
			if (Fp2.is_GF()) {
				Fp3.add_bool(false);
				Fp4.add_binar_op(op_V, Fp3, Fp1);
				rewrite_or(&Fp4, &Fp2, FV);
				return;
			}
			if (Fp1.is_GF()) {   
				Fp3.add_bool(false);
				Fp4.add_binar_op(op_V, Fp3, Fp2);  
				rewrite_or(&Fp1, &Fp4, FV);
				return;   
			}
			// G(Fp1 || FG Fp2) = G Fp1 || FG Fp2
			if (Fp2.is_FG()) {
				Fp3.add_bool(false);
				Fp4.add_binar_op(op_V, Fp3, Fp1);
				rewrite_or(&Fp4, &Fp2, FV);
				return;
			}
			if (Fp1.is_FG()) {
				Fp3.add_bool(false);
				Fp4.add_binar_op(op_V, Fp3, Fp2);
				rewrite_or(&Fp1, &Fp4, FV);
				return;
			}
			Fp1.clear(); Fp2.clear();
		}
	}

	// GX Fp1 = XG Fp1
        if ((FV1.what == LTL_BOOL) && (FV2.what == LTL_NEXT_OP)) {  
                if (!FV1.obsah.o3) {
                        FV2.get_next_op(Fp1);
                        Fp2.add_bool(false);
                        Fp3.add_binar_op(op_V, Fp2, Fp1);
                        rewrite_X(&Fp3, FV);
                        return;
                }
        }                

	//FV1 < !FV2 => (FV1 R FV2) = false R FV2
	if (eql(FV1, FV2.negace())) {
		F_pom.add_bool(false);
		FV.add_binar_op(op_V, F_pom, FV2); return;
	}

	// XFp1 R XFp2 = X(Fp1 R Fp2)
	if (FV1.get_next_op(Fp1) && FV2.get_next_op(Fp2)) {
		F_pom.add_binar_op(op_V, Fp1, Fp2);
		rewrite_X(&F_pom, FV);
		return;
	}

	FV.add_binar_op(op_V, FV1, FV2);
}

void LTL_formul_t::rewrite_X(LTL_formul_t *F1, LTL_formul_t& FV)
{
	LTL_formul_t FV1, Fp1, Fp2, Fp3, Fp4;
	LTL_oper_t op;

	F1->rewrite(FV1);
	if (FV1.what == LTL_BOOL) { // X b = b
		FV = FV1; return;
	}

	// XGF Fp = GF Fp
	if (FV1.is_GF()) {
		FV = FV1; return;
	}

	// XFG Fp = FG Fp
	if (FV1.is_FG()) {
		FV = FV1; return;
	}

	// X(Fp1 && GF Fp2) = XFp1 && GF Fp2
	if (FV1.get_binar_op(op, Fp1, Fp2)) {
		if ((op == op_and) || (op == op_or)) {
			if (Fp2.is_GF() || Fp2.is_FG()) {
				Fp3.add_next_op(Fp1);
				Fp4.add_binar_op(op, Fp3, Fp2);
				Fp4.rewrite(FV); return;
			}
			if (Fp1.is_GF() || Fp1.is_FG()) {
				Fp3.add_next_op(Fp2);
				Fp4.add_binar_op(op, Fp1, Fp3);
				Fp4.rewrite(FV); return;
			}
		}
		Fp1.clear(); Fp2.clear(); Fp3.clear(); Fp4.clear();
	}

	FV.add_next_op(FV1);
}

void LTL_formul_t::rewrite(LTL_formul_t& a)
{
	a.clear();

	switch (what) {
	case LTL_LITERAL:
	case LTL_BOOL:
		a = *this;
		return; break;
	case LTL_BIN_OP:
		switch (obsah.o1->op) {
		case op_and:
			rewrite_and(obsah.o1->arg1, obsah.o1->arg2, a);
			return; break;
		case op_or:
			rewrite_or(obsah.o1->arg1, obsah.o1->arg2, a);
			return; break;
		case op_U:
			rewrite_U(obsah.o1->arg1, obsah.o1->arg2, a);
			return; break;
		case op_V:
			rewrite_V(obsah.o1->arg1, obsah.o1->arg2, a);
			return; break;
		}
		break;
	case LTL_NEXT_OP:
		rewrite_X(obsah.o4, a);
		return; break;
	}
}

/* Prepsani LTL_formul_te; varianta 2 */

bool LTL_formul_t::is_G()
{
	if (what == LTL_BIN_OP) {
		if ((obsah.o1->op == op_V) &&
			(obsah.o1->arg1->what == LTL_BOOL)) {
			if (obsah.o1->arg1->obsah.o3 == false)
				return(true);
		}
	}

	return(false);
}

bool LTL_formul_t::is_F()
{
	if (what == LTL_BIN_OP) {
		if ((obsah.o1->op == op_U) &&
			(obsah.o1->arg1->what == LTL_BOOL)) {
			if (obsah.o1->arg1->obsah.o3 == true)
				return(true);
			}
	}

	return(false);
}

bool LTL_formul_t::is_pure_event()
{
	if (is_F()) return(true);

	switch (what) {
	case LTL_LITERAL:
	case LTL_BOOL:
		return(false); break;
	case LTL_NEXT_OP:
		return(obsah.o4->is_pure_event()); break;
	case LTL_BIN_OP:
		switch (obsah.o1->op) {
		case op_and:
		case op_or:
			return(obsah.o1->arg1->is_pure_event() &&
				obsah.o1->arg2->is_pure_event());
			break;
		case op_U:
			return(obsah.o1->arg1->is_pure_event());
			// na druhem argumentu nezalezi
			break;
		case op_V:
			if (is_G())
				return(obsah.o1->arg2->is_pure_event());
			return(obsah.o1->arg1->is_pure_event() &&
				obsah.o1->arg2->is_pure_event());
			break;
                default: gerr << "Unexpected value of variable \"obsah.o1->op\"" << thr();
                return false; //unreachable
                break;
		}
		break;
        default: gerr << "Unexpected value of variable \"what\"" << thr();
            return false; //unreachable
            break;
	}
}

bool LTL_formul_t::is_pure_universal()
{
	if (is_G()) return(true);

	switch (what) {
	case LTL_LITERAL:
	case LTL_BOOL:
		return(false); break;
	case LTL_NEXT_OP:
		return(obsah.o4->is_pure_universal()); break;
	case LTL_BIN_OP:
		switch (obsah.o1->op) {
		case op_and:
		case op_or:
			return(obsah.o1->arg1->is_pure_universal() &&
				obsah.o1->arg2->is_pure_universal());
			break;
		case op_V:
			return(obsah.o1->arg1->is_pure_universal());
			break;
		case op_U:
			if (is_F())
			return(obsah.o1->arg2->is_pure_universal());
			return(obsah.o1->arg1->is_pure_universal() &&
				obsah.o1->arg2->is_pure_universal());
			break;
                default: gerr << "Unexpected value of variable \"obsah.o1->op\"" << thr();
                return false; //unreachable
                break;
		}
		break;
        default: gerr << "Unexpected value of variable \"what\"" << thr();
            return false; //unreachable
            break;
	}
}

void LTL_formul_t::rewrite2_and(LTL_formul_t *F1, LTL_formul_t *F2,
	LTL_formul_t& FV)
{
	LTL_formul_t FV1, FV2, *f1, *f2, *f3, *f4, f_pom;

	F1->rewrite2(FV1); F2->rewrite2(FV2);

	if ((FV1.what == LTL_BIN_OP) && (FV2.what == LTL_BIN_OP)) {
		f1 = FV1.obsah.o1->arg1;
		f2 = FV1.obsah.o1->arg2;
		f3 = FV2.obsah.o1->arg1;
		f4 = FV2.obsah.o1->arg2;
		if ((FV1.obsah.o1->op == op_U) &&
			(FV2.obsah.o1->op == op_U)) {
			if (*f2 == *f4) {
				f_pom.add_binar_op(op_and, *f1, *f3);
				FV.add_binar_op(op_U, f_pom, *f2);
				return;
			}
		}
		if ((FV1.obsah.o1->op == op_V) &&
			(FV2.obsah.o1->op == op_V)) {
			if (*f1 == *f3) {
				f_pom.add_binar_op(op_and, *f2, *f4);
				FV.add_binar_op(op_V, *f1, f_pom);
				return;
			}
		}
	}

	FV.add_binar_op(op_and, FV1, FV2);
}

void LTL_formul_t::rewrite2_or(LTL_formul_t *F1, LTL_formul_t *F2,
	LTL_formul_t& FV)
{
	LTL_formul_t FV1, FV2, *f1, *f2, *f3, *f4, f_pom;

	F1->rewrite2(FV1); F2->rewrite2(FV2);

	if ((FV1.what == LTL_BIN_OP) && (FV2.what == LTL_BIN_OP)) {
		f1 = FV1.obsah.o1->arg1;
		f2 = FV1.obsah.o1->arg2;
		f3 = FV2.obsah.o1->arg1;
		f4 = FV2.obsah.o1->arg2;
		if ((FV1.obsah.o1->op == op_U) &&
			(FV2.obsah.o1->op == op_U)) {
			if (*f1 == *f3) {
				f_pom.add_binar_op(op_or, *f2, *f4);
				FV.add_binar_op(op_U, *f1, f_pom);
				return;
			}
		}
		if ((FV1.obsah.o1->op == op_V) &&
			(FV2.obsah.o1->op == op_V)) {
			if (*f2 == *f4) {
				f_pom.add_binar_op(op_or, *f1, *f3);
				FV.add_binar_op(op_V, f_pom, *f2);
				return;
			}
		}
	}

	FV.add_binar_op(op_or, FV1, FV2);
}

void LTL_formul_t::rewrite2_U(LTL_formul_t *F1, LTL_formul_t *F2,
	LTL_formul_t& FV)
{
	LTL_formul_t FV1, FV2;

	F1->rewrite2(FV1); F2->rewrite2(FV2);

	if (FV2.is_pure_event()) {
		FV = FV2; return;
	}
	if ((FV1.what == LTL_BOOL) && (FV2.what == LTL_BIN_OP)) {
		if ((FV1.obsah.o3) && (FV2.obsah.o1->op == op_U)) {
			FV.add_binar_op(op_U, FV1, *(FV2.obsah.o1->arg2));
			return;
		}
	}

	FV.add_binar_op(op_U, FV1, FV2);
}

void LTL_formul_t::rewrite2_V(LTL_formul_t *F1, LTL_formul_t *F2,
	LTL_formul_t& FV)
{
	LTL_formul_t FV1, FV2;

	F1->rewrite2(FV1); F2->rewrite2(FV2);

	if (FV2.is_pure_universal()) {
		FV = FV2; return;
	}
	if ((FV1.what == LTL_BOOL) && (FV2.what == LTL_BIN_OP)) {
		if (!(FV1.obsah.o3) && (FV2.obsah.o1->op == op_V)) {
			FV.add_binar_op(op_V, FV1, *(FV2.obsah.o1->arg2));
			return;
		}
	}

	FV.add_binar_op(op_V, FV1, FV2);
}

void LTL_formul_t::rewrite2_X(LTL_formul_t *F1, LTL_formul_t& FV)
{
	LTL_formul_t FV1;

	F1->rewrite2(FV1);
	FV.add_next_op(FV1);
}

void LTL_formul_t::rewrite2(LTL_formul_t& F)
{
	F.clear();

	switch (what) {
	case LTL_LITERAL:
	case LTL_BOOL:
		F = *this; return; break;
	case LTL_NEXT_OP:
		rewrite2_X(obsah.o4, F);
		return; break;
	case LTL_BIN_OP:
		switch (obsah.o1->op) {
		case op_and:
			rewrite2_and(obsah.o1->arg1, obsah.o1->arg2, F);
			return; break;
		case op_or:
			rewrite2_or(obsah.o1->arg1, obsah.o1->arg2, F);
			return; break;
		case op_U:
			rewrite2_U(obsah.o1->arg1, obsah.o1->arg2, F);
			return; break;
		case op_V:
			rewrite2_V(obsah.o1->arg1, obsah.o1->arg2, F);
			return; break;
		}
		break;
	}
}

/* ***** */

bool LTL_formul_t::operator==(const LTL_formul_t& a) const
{
	return(srovnej(a));
}

bool LTL_formul_t::operator!=(const LTL_formul_t& a) const
{
	bool v;

	v = srovnej(a);
	if (v)
		return(false);
	else
		return(true);
}

LTL_formul_t& LTL_formul_t::operator=(const LTL_formul_t& a)
{
	if (what != LTL_EMPTY) uvolni();
	kopiruj(&a);
	return(*this);
}

ostream& divine::operator<<(ostream& vystup, LTL_formul_t a)
{
	return(a.vypis(vystup));
}


/* Pomocne funkce */

bool divine::LTL_equiv_labels(const list<LTL_formul_t>& L1,
	const list<LTL_formul_t>& L2)
{
	list<LTL_formul_t>::const_iterator l_b1, l_e1, l_i1, l_b2, l_e2, l_i2;

	if (L1.size() != L2.size()) return(false);

	l_b1 = L1.begin(); l_e1 = L1.end();
	l_b2 = L2.begin(); l_e2 = L2.end();

	for (l_i1 = l_b1; l_i1 != l_e1; l_i1++) {
		l_i2 = find(l_b2, l_e2, *l_i1);
		if (l_i2 == l_e2) return(false);
	}
	for (l_i2 = l_b2; l_i2 != l_e2; l_i2++) {
		l_i1 = find(l_b1, l_e1, *l_i2);   
		if (l_i1 == l_e1) return(false);
	}

	return(true);
}

bool divine::LTL_equiv_labels(const list<LTL_literal_t>& L1,
	const list<LTL_literal_t>& L2)
{
	list<LTL_literal_t>::const_iterator l_b1, l_e1, l_i1, l_b2, l_e2, l_i2;

	if (L1.size() != L2.size()) return(false);

	l_b1 = L1.begin(); l_e1 = L1.end();
	l_b2 = L2.begin(); l_e2 = L2.end();

	for (l_i1 = l_b1; l_i1 != l_e1; l_i1++) {
		l_i2 = find(l_b2, l_e2, *l_i1);
		if (l_i2 == l_e2) return(false);
	}
	for (l_i2 = l_b2; l_i2 != l_e2; l_i2++) {
		l_i1 = find(l_b1, l_e1, *l_i2);   
		if (l_i1 == l_e1) return(false);
	}

	return(true);
}


bool divine::LTL_subset_label(const list<LTL_formul_t>& L1,
	const list<LTL_formul_t>& L2)
{
	list<LTL_formul_t>::const_iterator l_b1, l_e1, l_i1, l_b2, l_e2, l_i2;

	if (L1.size() > L2.size()) return(false);

	l_b1 = L1.begin(); l_e1 = L1.end();
	l_b2 = L2.begin(); l_e2 = L2.end();

	for (l_i1 = l_b1; l_i1 != l_e1; l_i1++) {
		l_i2 = find(l_b2, l_e2, *l_i1);
		if (l_i2 == l_e2) break;
	}
	if (l_i1 == l_e1)
		return(true);
	else
		return(false);
}

bool divine::LTL_subset_label(const list<LTL_literal_t>& L1,
	const list<LTL_literal_t>& L2)
{
	list<LTL_literal_t>::const_iterator l_b1, l_e1, l_i1, l_b2, l_e2, l_i2;

	if (L1.size() > L2.size()) return(false);

	l_b1 = L1.begin(); l_e1 = L1.end();
	l_b2 = L2.begin(); l_e2 = L2.end();

	for (l_i1 = l_b1; l_i1 != l_e1; l_i1++) {
		l_i2 = find(l_b2, l_e2, *l_i1);
		if (l_i2 == l_e2) break;
	}
	if (l_i1 == l_e1)
		return(true);
	else
		return(false);
}


bool divine::LTL_is_contradiction(list<LTL_formul_t>& L)
{
	list<LTL_formul_t>::iterator l_b, l_e, l_i1, l_i2;
	LTL_formul_t F;

	l_b = L.begin(); l_e = L.end();
	for (l_i1 = l_b; l_i1 != l_e; l_i1++) {
		F = l_i1->negace();
		for (l_i2 = l_b; l_i2 != l_e; l_i2++) {
			if (l_i1 == l_i2) continue;
			if (F == *l_i2) return(true);
		}
	}

	return(false);
}

bool divine::LTL_is_contradiction(const list<LTL_literal_t>& L)
{
	list<LTL_literal_t>::const_iterator l_b, l_e, l_i1, l_i2;
	LTL_literal_t Lit;

	l_b = L.begin(); l_e = L.end();
	for (l_i1 = l_b; l_i1 != l_e; l_i1++) {
		Lit.negace = !(l_i1->negace);
		Lit.predikat = l_i1->predikat;
		for (l_i2 = l_b; l_i2 != l_e; l_i2++) {
			if (l_i1 == l_i2) continue;
			if (Lit == *l_i2) return(true);
		}
	}

	return(false);
}


/*
*  class for representing LTL formulae
*    - LTL formula is representing in "positive normal form", i.e. formula
*      contains only atomic proposition or negated atomic proposition
*      and operators && (boolean and), || (boolean or), U (until) and
*      R (release)
*
*  Milan Jaros - xjaros1@fi.muni.cz
*
*/

#ifndef DIVINE_FORMUL_HH
#define DIVINE_FORMUL_HH

#ifndef DOXYGEN_PROCESSING

#include <iostream>
#include <string>
#include <list>

//#include "deb/deb.hh"

using namespace std;

namespace divine {

#endif // DOXYGEN_PROCESSING

/* Binary operators: */
enum LTL_oper_t {op_and, op_or, op_U, op_V};

class LTL_formul_t;

/* binary operator node */
struct LTL_binar_op_t { // (*arg1) op (*arg2)
	LTL_oper_t op;
	LTL_formul_t *arg1, *arg2;
};

/* literal node */
struct LTL_literal_t {
	bool negace; // false = negation
	string predikat;

	inline bool operator==(const LTL_literal_t& L) const {
		return((negace == L.negace) &&
			(predikat == L.predikat));
	}

	inline bool operator!=(const LTL_literal_t& L) const {
		return((negace != L.negace) ||
			(predikat != L.predikat));
	}
        inline bool operator<(const LTL_literal_t& L) const {
		
                if ( predikat==L.predikat )
                 {
                   return negace < L.negace;   
                 }
                return( predikat < L.predikat);
	}
};

std::ostream& operator<<(std::ostream& vystup, LTL_literal_t L);

typedef list<LTL_literal_t> LTL_label_t;

/* Type of formula (returned by method typ()): */
#define LTL_EMPTY   0 // Empty formula
#define LTL_BIN_OP  1 // Formula with binary operator
#define LTL_LITERAL 2 // Atomic propozition and negation of at. prop.
#define LTL_BOOL    3 // 'true' or 'false'
#define LTL_NEXT_OP 4 // Formula with operator X (next)

/* Setting of writing operators on output */
#define LTL_OUT_AND " && " // Logical and ( & * ...)
#define LTL_OUT_OR " || "  // or (| + )
#define LTL_OUT_U " U "    // until
#define LTL_OUT_V " R "    // release (R)
#define LTL_OUT_X "X "     // next (O)
#define LTL_OUT_NOT "!"    // negation

/* LTL formula */
class LTL_formul_t {
private:
	union {
		LTL_binar_op_t *o1;
		LTL_literal_t *o2;
		bool o3;
		LTL_formul_t *o4;
	} obsah;
	int what; // Type of formula

	/* helping methods: */
	void kopiruj(const LTL_formul_t *a);
	bool srovnej(const LTL_formul_t& a) const;
	void uvolni();

	std::ostream& vypis(std::ostream& vystup);

	/* Methods for rewriting of formula - variant 1 */
	bool eql(LTL_formul_t& F1, LTL_formul_t& F2); // F1 <= F2

	bool is_GF();
	bool is_GF(LTL_formul_t& F1);
	bool is_FG();
	bool is_FG(LTL_formul_t& F1);

	void rewrite_and(LTL_formul_t *F1, LTL_formul_t *F2,
		LTL_formul_t& FV);
	void rewrite_or(LTL_formul_t *F1, LTL_formul_t *F2,
		LTL_formul_t& FV);
	void rewrite_U(LTL_formul_t *F1, LTL_formul_t *F2,
		LTL_formul_t& FV);
	void rewrite_V(LTL_formul_t *F1, LTL_formul_t *F2,
		LTL_formul_t& FV);
	void rewrite_X(LTL_formul_t *F1, LTL_formul_t& FV);

	/* rewriting formula - variant 2 */
	bool is_G();
	bool is_F();

	bool is_pure_event();
	bool is_pure_universal();

	void rewrite2_and(LTL_formul_t *F1, LTL_formul_t *F2,
		LTL_formul_t& FV);
	void rewrite2_or(LTL_formul_t *F1, LTL_formul_t *F2,
		LTL_formul_t& FV);
	void rewrite2_U(LTL_formul_t *F1, LTL_formul_t *F2,
		LTL_formul_t& FV);
	void rewrite2_V(LTL_formul_t *F1, LTL_formul_t *F2,
		LTL_formul_t& FV);
	void rewrite2_X(LTL_formul_t *F1, LTL_formul_t& FV);

public:
	LTL_formul_t(); // constructor
	LTL_formul_t(const LTL_formul_t& a); // copy constructor
	~LTL_formul_t(); // destructor

	void clear(); // deleting formula

	/* Methods for replacing actual instance */
	void add_literal(string pred);
	void add_literal(bool neg, string pred); //neg = false -> negation
	void add_binar_op(LTL_oper_t op, LTL_formul_t& arg1,
		LTL_formul_t& arg2);
	void add_bool(bool b);
	void add_next_op(LTL_formul_t& arg);

	/* Methods for geting contents of formula */
	int typ() const; // type of formula

	/* if type of formula is correct, methods will return 'true' and
	requested information in their arguments. In other case, methods
	will return 'false' */
	bool get_literal(bool& neg, string& pred) const;
	bool get_binar_op(LTL_oper_t& op, LTL_formul_t& arg1,
		LTL_formul_t& arg2) const;
	bool get_binar_op(LTL_oper_t& op) const;
	bool get_bool(bool& b) const;
	bool get_next_op(LTL_formul_t& arg) const;

	/* negation of formula (in actual instance) */
	LTL_formul_t& negace();

	/* rewriting the formula (in actual instace), result is returned
	in argument of methods */
	void rewrite(LTL_formul_t& F); // variant 1
	void rewrite2(LTL_formul_t& F); // variant 2

	/* Syntactic comparsion of formulae: */
	bool operator==(const LTL_formul_t& a) const;
	bool operator!=(const LTL_formul_t& a) const;


	LTL_formul_t& operator=(const LTL_formul_t& a);

	/* writing the formula to output */
	friend std::ostream& operator<<(std::ostream& vystup,
		LTL_formul_t F);
};


/* helping, useful functions */

/* comparsion of 2 lists of formulae */
bool LTL_equiv_labels(const list<LTL_formul_t>& L1,
	const list<LTL_formul_t>& L2);
bool LTL_equiv_labels(const list<LTL_literal_t>& L1,
	const list<LTL_literal_t>& L2);

/* 'true' is returned when L1 \subseteq L2 */
bool LTL_subset_label(const list<LTL_formul_t>& L1,
	const list<LTL_formul_t>& L2);
bool LTL_subset_label(const list<LTL_literal_t>& L1,
	const list<LTL_literal_t>& L2);

/* 'true' is returned when label is contradiction (for example <p, !p>) */
bool LTL_is_contradiction(list<LTL_formul_t>& L);
bool LTL_is_contradiction(const list<LTL_literal_t>& L);

#ifndef DOXYGEN_PROCESSING
}

//#include "deb/undeb.hh"

#endif // DOXYGEN_PROCESSING

#endif // DIVINE_FORMUL_HH

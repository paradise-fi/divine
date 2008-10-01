/*
*  Syntax of LTL formulae, translation to instance of class LTL_formul_t
*
*  Milan Jaros - xjaros1@fi.muni.cz
*
*/

#ifndef DIVINE_LTL_PARSE
#define DIVINE_LTL_PARSE

#ifndef DOXYGEN_PROCESSING

#include "formul.hh"
#include "buchi_lang.hh"

//#include "deb/deb.hh"

namespace divine {
#endif //DOXYGEN_PROCESSING

enum LTL_syntax_el_t {ltl_box, ltl_diamond, ltl_not, ltl_next, ltl_until,
	ltl_V, ltl_and, ltl_or, ltl_xor, ltl_impl, ltl_ekv, ltl_weak,
	ltl_true, ltl_false, ltl_atom, left_par, right_par, unknown, end};
/* Syntactic elements - unary operators, binary operators, true,
	false, atom, parenthesis, unknown element, end of formula
*/

/* LTL formula - parser */

class LTL_parse_t {
private:
	string formule;
        
	/* helping method */
	LTL_syntax_el_t get_syntax_el(int& i, string& pred);

public:
	LTL_parse_t(); // constructor
	LTL_parse_t(const LTL_parse_t& a); // copy constructor
	/* constructor with reading string as a formula */
	LTL_parse_t(string& s);
	~LTL_parse_t(); // destructor

	/* syntax checking of readed formula and translation to instance
	of class LTL_formul_t (in positive normal form) */
	bool syntax_check(LTL_formul_t& F);

	/* reading formula from string */
	void nacti(string& s);

	/* Nacteni formule ze souboru v nasledujicim formatu:
		* radek zacinajici znakem '#' je komentar
		* radek zacinajici znaky '#@' je komentar, ktery muze byt
		vypsan do vystupniho souboru (kde zacina opet '#@')
		* formule muze byt na vice radcich, mezi temito radky
		mohou byt komentare; platne radky jsou spojeny do jednoho
	*/
	/* reading formula form text file in following format:
		* line with first character '#' is comment
		* line with first two characters '#@' is comment and this
		comment can be writen to output file
		* file contains 1 formula. This formula can be writen on
		several lines. Between this lines can be writen comments.
	*/
	void nacti(std::istream& fr, std::ostream& vystup);

	/* reading formula for text file without writing to output file */
	void nacti(std::istream& fr);

	/* "Interactive" reading formula from 'cin' */
	void nacti(void);

	/* writing readed formula to 'cout' */
	void vypis(void);

	/* Operators for reading and writing formula
		- only one line with formula
	*/
	friend std::istream& operator>>(std::istream& vstup,
		LTL_parse_t& F);
	friend std::ostream& operator<<(std::ostream& vystup,
		LTL_parse_t F);
};

#ifndef DOXYGEN_PROCESSING
} //END of namespace DVE

//#include "deb/undeb.hh"

#endif // DOXYGEN_PROCESSING

#endif // DIVINE_LTL_PARSE

/*
 * This file is part of the program ltl2dstar (http://www.ltl2dstar.de/).
 * Copyright (C) 2005-2007 Joachim Klein <j.klein@ltl2dstar.de>
 * Modified 2011 Jiri Appl <jiri@appl.name>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 *  published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef LTL2NBA_HPP
#define LTL2NBA_HPP

/** @file
 * Provides wrapper classes for external LTL-to-Buechi translators.
 */

#include "NBA.hpp"
#include "LTLFormula.hpp"
#include "parsers/parser_interface.hpp"
#include "common/TempFile.hpp"
#include <cstdio>

/**
 * Virtual base class for wrappers to external LTL-to-Buechi translators.
 */
template <class NBA_t>
class LTL2NBA {
public:
  /** Constructor */
  LTL2NBA() {}
  /** Destructor */
  virtual ~LTL2NBA() {}

  /** Convert an LTL formula to an NBA */
  virtual NBA_t *ltl2nba(LTLFormula& ltl) = 0;
};

#ifdef O_LTL3BA
  extern int main_ltl3ba(int arc, char* argv[], FILE* outFile);
#else
  extern "C" int main_ltl2ba(int arc, char* argv[], FILE* outFile);
#endif

/**
 * Wrapper for linked LTL-to-Buechi translators using the SPIN interface.
 */
template <class NBA_t>
class LTL2NBA_SPINint : public LTL2NBA<NBA_t> {
public:
	/**
	* Constructor
	* @param path dummy holder
	* @param arguments vector of command line arguments to be passed to the internal translator
	*/
	LTL2NBA_SPINint(std::string path, 
		std::vector<std::string> arguments=std::vector<std::string>()) :
		_arguments(arguments)  {}
	
	/** Destructor */
	virtual ~LTL2NBA_SPINint() {}
	
	/** 
	* Convert an LTL formula to an NBA
	* @param ltl
	* @return a pointer to the created NBA (caller gets ownership).
	*/
	virtual NBA_t *ltl2nba(LTLFormula& ltl) {
		// Create canonical APSet (with 'p0', 'p1', ... as AP)
		LTLFormula_ptr ltl_canonical=ltl.copy();
		APSet_cp canonical_apset(ltl.getAPSet()->createCanonical());
		ltl_canonical->switchAPSet(canonical_apset);
	
		AnonymousTempFile spin_outfile;
		std::vector<std::string> arguments;
#ifdef O_LTL3BA
		arguments.push_back("ltl3ba");
#else
		arguments.push_back("ltl2ba");
#endif
		arguments.push_back("-f");
		arguments.push_back(ltl_canonical->toStringInfix());
	
		arguments.insert(arguments.end(), _arguments.begin(), _arguments.end());
		char** argv = new char*[ arguments.size() ];
		unsigned i = 0;
		for ( std::vector<std::string>::iterator it = arguments.begin(); it != arguments.end(); ++it, i++)
			argv[ i ] = const_cast<char*>(it->c_str());
		
#ifdef O_LTL3BA
		main_ltl3ba(arguments.size(), argv, spin_outfile.getOutFILEStream());
#else
		main_ltl2ba(arguments.size(), argv, spin_outfile.getOutFILEStream());
#endif
		
		NBA_t *result_nba(new NBA_t(canonical_apset));
		
		FILE *f=spin_outfile.getInFILEStream();
		if (f == NULL)
			throw Exception("");
	
		int rc=nba_parser_promela::parse(f, result_nba);
		fclose(f);
	
		if (rc != 0)
			throw Exception("Couldn't parse PROMELA file!");
		
		// switch back to original APSet
		result_nba->switchAPSet(ltl.getAPSet());
	
		return result_nba;
	}

private:
	/** The arguments */
	std::vector<std::string> _arguments;
};
#endif

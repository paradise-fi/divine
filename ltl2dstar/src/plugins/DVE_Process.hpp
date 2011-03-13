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

#include "plugins/PluginManager.hpp"

typedef DRA_t::state_type state_type;
using std::endl;

class DVEProcessPlugin : public PluginManager::Plugin {
private:
	bool firstTransition;
	
	std::string stateNameFromIndex( DRA_t& dra, const unsigned index ) const {
		state_type* cur_state = dra.get(index);
		return stateName( cur_state );
	}
	
	std::string stateName( const state_type* const state) const {
		std::ostringstream str;
		str << "q" << state->getName();
		return str.str();
	}
	
	void printTransition( std::ostream& out, const state_type* const from, const state_type* const to, const std::string& guards ) {
		if (this->firstTransition)
			this->firstTransition = false;
		else
			out << "," << endl;
		
		out << stateName( from ) << " -> " << stateName( to );
		
		out << " {";
		if (guards != "")
			out << " guard " << guards << "; ";
		out << "}";
	}
    
	std::string getGuard( const APSet &ap_set, const APElement& elem ) const {
		if ( ap_set.size() == 0 ) 
			return "";
		
		std::ostringstream r;
		for ( unsigned i = 0; i < ap_set.size(); i++ ) {
			if ( i >= 1 ) 
				r << " && ";
			if ( !elem.get(i) ) 
				r << " not ";
			r << ap_set.getAP(i);
		}
		return r.str();
	}
	
	void addState ( DRA_t& dra, const unsigned index, std::ostringstream& stateList ) const {
		if ( stateList.str().size() != 0 )
			stateList << ", ";
		stateList << stateNameFromIndex( dra, index );
	}
	
public:
	DVEProcessPlugin() : PluginManager::Plugin( "DVE_Process" ), firstTransition( true ) {}

	virtual result_t outputDRA( DRA_t& dra, std::ostream& out ) {
		out << "process LTL_property {" << endl << "state ";
	
		for ( unsigned i = 0; i < dra.size(); i++ ) {
			if ( i != 0 )
				out << ", ";
			
			out << stateNameFromIndex( dra, i );
		}
		
		out << ";" << endl;
		out << "init " << stateName( dra.getStartState() ) << ";" << endl;
		
		out << "accept rabin "; // ( , , , ; , , , ), ( ; );
		const RabinAcceptance& acceptance = dra.acceptance();
		for ( unsigned j = 0; j < acceptance.size(); j++ ) {
			std::ostringstream Lstatelist, Ustatelist;
			for ( unsigned i = 0; i < dra.size(); i++ ) {
				if ( acceptance.isStateInAcceptance_L( j, i ) )
					addState( dra, i, Lstatelist );
				if ( acceptance.isStateInAcceptance_U( j, i ) )
					addState( dra, i, Ustatelist );
			}
			
			if ( j != 0 )
				out << ", ";
			out << "( " << Lstatelist.str() << " ; " << Ustatelist.str() << " )";
		}
		out << ";" << endl;
		
		out << "trans" << endl;
		
		const APSet& ap_set = dra.getAPSet();
		
		for ( unsigned i = 0; i < dra.size(); i++ ) {
			state_type* cur_state = dra.get(i);
		
			if ( cur_state->hasOnlySelfLoop() ) {
				// get first to-state, as all the to-states are the same
				state_type *to = cur_state->edges().get( *( ap_set.all_elements_begin() ) );
				
				printTransition( out, cur_state, to, "" );
			} else {
				for ( APSet::element_iterator el_it = ap_set.all_elements_begin(); el_it != ap_set.all_elements_end(); ++el_it ) {
					APElement label=*el_it;
					state_type *to_state=cur_state->edges().get( label );
					printTransition( out, cur_state, to_state, getGuard( ap_set, label ) );
				}
			}
		}
		
		if ( !this->firstTransition )
			out << ";" << endl;
		out << "}" << endl;
	
		return PLUGIN_CONTINUE;
	}
};
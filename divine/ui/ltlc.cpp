// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Tadeáš Kučera <kucerat@mail.muni.cz>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <divine/ltl/buchi.hpp>
#include <divine/ltl/ltl.hpp>
#include <divine/ui/cli.hpp>

using namespace std::literals;

namespace divine {
namespace ui {
    /*
     * creates file containing monitor for given _formula and users program 
     * CMD: ./test/divine ltlc -o monitor.cpp -f 'y & x & z & q'
     */

    using namespace ltl;
    using LTLPtr = std::shared_ptr< LTL >;

    void Ltlc::run()
    {
        TGBA2 tgba2;
        if( !_automaton.empty() )
            tgba2 = HOAParser( _automaton );
        else if( !_formula.empty() ) {
            LTLPtr parsedF = LTL::parse( _formula, true );
            TGBA1 tgba1 = ltlToTGBA1( parsedF, _negate );
            tgba2 = std::move( tgba1 );
        }
        else {
            std::cerr << "Verified property description missing: --automaton or --formula required" << std::endl;
            return;
        }
        std::cout << tgba2;

        std::stringstream o;
            o   << "// Created by /home/x423907/divine/divine/ui/ltlc.cpp" << std::endl
                << std::endl
                << "#include <algorithm>" << std::endl
                << "#include <dios.h>" << std::endl
                << "#include <optional>" << std::endl
                << "#include <iostream>" << std::endl
                << "#include <set>" << std::endl
                << "#include <vector>" << std::endl
                << std::endl
                << "#include \"" << _system << "\"" << std::endl
                << std::endl
                << "/*" << std::endl
                << "* *******************************************************************" << std::endl
                << "* This file contains monitor generated from LTL formula " << _formula << std::endl
                << "* *******************************************************************" << std::endl
                << "* */" << std::endl
                << std::endl
                << std::endl
                << "struct TGBA" << std::endl
                << "{" << std::endl
                << "    size_t current = " << tgba2.start << ";" << std::endl
                << "    size_t nStates = " << tgba2.nStates << ";" << std::endl
                << "    size_t numberAcc = " << tgba2.nAcceptingSets << ";" << std::endl
                << std::endl
                << "    std::optional< size_t > acceptingSCC( size_t state )" << std::endl
                << "    {" << std::endl
                << "        std::optional< size_t > out;" << std::endl
                << "        switch( state ) {" << std::endl;
        for( size_t state = 0; state < tgba2.nStates; ++state )
            if( tgba2.accSCC.at( state ).has_value() )
            {
                o   << "            case " << state << ":" << std::endl
                    << "                out = " << tgba2.accSCC.at( state ).value() << ";" << std::endl;
            }
            o   << "        }" << std::endl
                << "        return out;" << std::endl
                << "    }" << std::endl
                << "    bool step( std::set< size_t >& acceptingSets, std::optional< size_t >& scc1, std::optional< size_t >& scc2, size_t& dest )" << std::endl
                << "    {" << std::endl
                << "        std::vector< size_t > reachable;" << std::endl
                << "        switch( current ) {" << std::endl;
        for( size_t state = 0; state < tgba2.states.size(); ++state ) {
            o   << "            case " << state << ":" << std::endl;
            for( const auto& trans : tgba2.states.at( state ) ) { //all the succesors of state
                if( trans.label.empty() )
                o   << "                reachable.push_back( " << trans.target << " );" << std::endl;
                else {
                o   << "                if( ";
                for( auto l = trans.label.begin(); l != trans.label.end(); ) {
                    if( !l->first )
                        o   << "!";
                    o   << "prop_" << tgba2.allTrivialLiterals.at( l->second )->string() << "()";
                    ++l;
                    if( l != trans.label.end() )
                        o   << " && ";
                }
            o   <<                                    " )" << std::endl
                << "                    reachable.push_back( " << trans.target << " );" << std::endl;
                }
            }
            o   << "                break;" << std::endl;
        }
            o   << "            default:" << std::endl
                << "                assert( false && \"state does not exist\" );" << std::endl
                << "        }" << std::endl
                << "        size_t newState = -1;" << std::endl
                << "        if( reachable.empty() )" << std::endl
                << "            return false;" << std::endl
                << "        newState = reachable.at( monitor_choose( reachable.size() ) );" << std::endl
                << "        switch( current ) {" << std::endl;
        for( size_t current = 0; current < tgba2.states.size(); ++current ) {
            o   << "            case " << current << ":" << std::endl;
            for( const auto& trans : tgba2.states.at( current ) )
            if( !trans.accepting.empty() )
            {
            o   << "                if( newState == " << trans.target << " )" << std::endl
                << "                    acceptingSets.insert( { ";
                for( auto it = trans.accepting.begin(); it != trans.accepting.end(); )
                {
                    o << *it;
                    if( ++it != trans.accepting.end() )
                        o << ", ";
                }
            o   <<                                            " } );" << std::endl;
            }
            o   << "                break;" << std::endl;
        }
            o   << "            default:" << std::endl
                << "                assert( false && \" state does not exist \" );" << std::endl
                << "        }" << std::endl
                << "        scc1 = acceptingSCC( current );" << std::endl
                << "        scc2 = acceptingSCC( newState );" << std::endl
                << "        dest = newState;" << std::endl
                << "        current = newState;" << std::endl
                << "        return true;" << std::endl
                << "    }" << std::endl
                << "};" << std::endl << std::endl;
        std::ofstream outputFile;
        outputFile.open( _output );
        outputFile << o.str();
        outputFile.close();
    }
}
}

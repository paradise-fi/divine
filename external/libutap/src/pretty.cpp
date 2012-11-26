// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-

/* libutap - Uppaal Timed Automata Parser.
   Copyright (C) 2002 Uppsala University and Aalborg University.
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA
*/

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <string>

#include "utap/utap.h"
#include "utap/prettyprinter.h"

#if defined(__MINGW32__) || defined(__CYGWIN32__) || !defined(HAVE_UNISTD_H) 
extern "C" 
{
    extern int getopt(int argc, char * const argv[], const char *optstring);
    extern char *optarg;
    extern int optind, opterr, optopt;
}
#endif

using UTAP::ParserBuilder;
using UTAP::TimedAutomataSystem;
using std::vector;
using std::endl;
using std::cout;
using std::cerr;

using namespace UTAP::Constants;

static bool newSyntax = (getenv("UPPAAL_OLD_SYNTAX") == NULL);

/**
 * Test for pretty printer
 */
int main(int argc, char *argv[])
{
    try 
    {
        std::string filename;

        if (argc != 2)
        {
            std::cerr << "Usage: " << argv[0] << " MODEL\n\n";
            std::cerr << "where MODEL is a UPPAAL .xml, xta, or .ta file\n";
            return 1;
        }

        filename = argv[1];

        UTAP::PrettyPrinter pretty(cout);

        if (strcasecmp(".xml", filename.c_str() + filename.length() - 4) == 0)
        {
            parseXMLFile(filename.c_str(), &pretty, newSyntax);
        }
        else 
        {
            FILE *file = fopen(filename.c_str(), "r");
            if (file == NULL) 
            {
                char msg[256];
                snprintf(msg, 255, "Error opening %s", filename.c_str());
                perror(msg);
                return 1;
            }
            
            parseXTA(file, &pretty, newSyntax);
        }
    } 
    catch (std::exception &e)
    {
        std::cerr << "Caught exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

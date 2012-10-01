/* -*- C++ -*-
 * mu.C
 * @(#) main() for the Murphi compiler.
 *
 * Copyright (C) 1992 - 1999 by the Board of Trustees of
 * Leland Stanford Junior University.
 *
 * License to use, copy, modify, sell and/or distribute this software
 * and its documentation any purpose is hereby granted without royalty,
 * subject to the following terms and conditions:
 *
 * 1.  The above copyright notice and this permission notice must
 * appear in all copies of the software and related documentation.
 *
 * 2.  The name of Stanford University may not be used in advertising or
 * publicity pertaining to distribution of the software without the
 * specific, prior written permission of Stanford.
 *
 * 3.  This software may not be called "Murphi" if it has been modified
 * in any way, without the specific prior written permission of David L.
 * Dill.
 *
 * 4.  THE SOFTWARE IS PROVIDED "AS-IS" AND STANFORD MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION.  STANFORD MAKES NO REPRESENTATIONS OR WARRANTIES
 * OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE
 * USE OF THE SOFTWARE WILL NOT INFRINGE ANY PATENTS, COPYRIGHTS
 * TRADEMARKS OR OTHER RIGHTS. STANFORD SHALL NOT BE LIABLE FOR ANY
 * LIABILITY OR DAMAGES WITH RESPECT TO ANY CLAIM BY LICENSEE OR ANY
 * THIRD PARTY ON ACCOUNT OF, OR ARISING FROM THE LICENSE, OR ANY
 * SUBLICENSE OR USE OF THE SOFTWARE OR ANY SERVICE OR SUPPORT.
 *
 * LICENSEE shall indemnify, hold harmless and defend STANFORD and its
 * trustees, officers, employees, students and agents against any and all
 * claims arising out of the exercise of any rights under this Agreement,
 * including, without limiting the generality of the foregoing, against
 * any damages, losses or liabilities whatsoever with respect to death or
 * injury to person or damage to property arising from or out of the
 * possession, use, or operation of Software or Licensed Program(s) by
 * LICENSEE or its customers.
 *
 * Read the file "license" distributed with these sources, or call
 * Murphi with the -l switch for additional information.
 * 
 */

/* 
 * Original Author: Ralph Melton
 * 
 * Update:
 *
 * Seungjoon Park
 * Subject: liveness syntax
 * Last modified: Novemeber  5, 93
 *
 * C. Norris Ip
 * Subject: Extension for {multiset, undefined value, general union}
 * First modified: May 24, 94
 *
 * Denis Leroy
 * Subject: 'no compression' option
 * Last modified: May 13, 95
 */ 

#include "mu.h"
#include <string.h>
#include <new>

using namespace std;

/********************
  variable declarations
  ********************/
program *theprog = NULL;     
symboltable *symtab = NULL;
FILE *codefile;

/********************
  void err_new_handler()
  ********************/
void err_new_handler()
{
  Error.FatalError("Unable to allocate enough memory.");
}

/********************
  void init_globals()
  -- initialize all global variables
  ********************/
void init_globals()
{
  set_new_handler(&err_new_handler);
  booltype = new enumtypedecl(0,1);
  booltype->declared = TRUE;
  booltype->mu_name = tsprintf("mu_0_boolean");
  inttype = new enumtypedecl(0, 10000);
  inttype->declared = TRUE;
  /* An enum, not a subrange, because an integer is a primitive type. */
  /* The upper bound doesn\'t really matter; it doesn\'t get used. MAXINT
   * seems like a logical value, but that causes an overflow in the
   * arithmetic used to calculate the number of bits it requires. I\'ve
   * chosen a value that shouldn\'t create any problems, even with small
   * ints. */
  errortype = new errortypedecl("ERROR");
  voidtype = new typedecl("VOID_TYPE");
  errorparam = new param(errortype);
  errordecl = new error_decl("ERROR_DECL");
  error_expr = new expr(0,errortype);
  true_expr = new expr(TRUE, booltype);
  false_expr = new expr(FALSE, booltype);
  error_designator = new designator(NULL, errortype, FALSE, FALSE, FALSE);
  nullstmt = new stmt;
  error_rule = new simplerule (NULL,NULL,NULL,NULL,FALSE, 0);
  symtab = new symboltable;
  theprog = new program;
  typedecl::origin = NULL;
}

/********************
  void setup_infile(char *filename)
  -- setup input file handler
  ********************/
void setup_infile(const char *filename)
{
  /* pre-checking on filename. */
  int len = strlen(filename);
  if ( filename[len - 2] != '.' ||
       filename[len - 1] != 'm' )
    {
      Error.FatalError("Murphi only handles files ending in \".m\"");
    }
  else
    {
      gFileName = filename;
      yyin = fopen(filename, "r"); // yyin is flex\'s global for the input file
      if ( yyin == NULL )
	{
	  Error.FatalError("%s:No such file or directory.", filename);
	}
    }
}

/********************
  FILE *setup_outfile(char *infilename)
  -- setup output file handler
  ********************/
FILE *setup_outfile(const char *out)
{
  FILE *outfile;

  outfile = fopen(out, "w");
  if ( outfile == NULL )
    {
      Error.FatalError("Unable to open/create %s",out);
    };
  return outfile;
}

/********************
  main routines
  ********************/
bool mucompile(const char *in, const char *out)
{
  int error;

  init_globals();
  setup_infile( in );
  error = yyparse();
  if ( !error && Error.getnumerrors() == 0 )
    {
      codefile = setup_outfile( out );
      theprog->generate_code();
      fclose(codefile);
      return true;
    }
  else
    {
        return false;
    }
}

/****************************************
  * 8 March 94 Norris Ip:
  merge with the latest rel2.6
  * 17 March 94 Norris Ip:
  fixed ruleset boolean segmentation fault
  ******
  * Subject: Extension for {multiset, undefined value, general union}
  * 25 May 94 Norris Ip:
  designator:: added maybeundefined field
****************************************/

/********************
 $Log: mu.C,v $
 Revision 1.3  1999/01/29 08:28:10  uli
 efficiency improvements for security protocols

 Revision 1.2  1999/01/29 07:49:13  uli
 bugfixes

 Revision 1.4  1996/08/07 18:54:00  ip
 last bug fix on NextRule/SetNextEnabledRule has a bug; fixed this turn

 Revision 1.3  1996/08/07 00:59:13  ip
 Fixed bug on what_rule setting during guard evaluation; otherwise, bad diagnoistic message on undefine error on guard

 Revision 1.2  1996/08/06 23:57:39  ip
 fixed while code generation bug

 Revision 1.1  1996/08/06 23:56:55  ip
 Initial revision

 ********************/


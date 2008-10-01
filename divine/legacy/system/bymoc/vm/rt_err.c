/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#include <stdio.h>
#include <stdlib.h>

#include "rt_err.h"


// generate a runtime error and abort program
void rt_err( char *err_msg ) // extern
{
  fprintf( stderr, "RUNTIME ERROR: %s\n", err_msg );
  exit( -1 );
}

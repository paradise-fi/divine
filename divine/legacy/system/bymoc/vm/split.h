/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#ifndef INC_split
#define INC_split


#ifdef __cplusplus
extern "C"
{
#endif


#include "state.h"


// split a state and recreate it from its parts - then compare it
//  - used to test state_parts
//  - outputs some info if stream != NULL
//  - returns boolean flag if success
extern int split_test( st_global_state_header * p_glob, FILE * stream );


#ifdef __cplusplus
} // extern "C"
#endif


#endif // #ifndef INC_split

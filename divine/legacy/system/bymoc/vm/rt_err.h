/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#ifndef INC_rt_err
#define INC_rt_err


#ifdef __cplusplus
extern "C"
{
#endif


// generate a runtime error and abort program
extern void rt_err( char *err_msg );


#ifdef __cplusplus
} // extern "C"
#endif


#endif // #ifndef INC_rt_err

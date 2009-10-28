/***************************************************************************
 *   Copyright (C) 2009 by Martin Moracek                                  *
 *   xmoracek@fi.muni.cz                                                   *
 *                                                                         *
 *   DiVinE is free software, distributed under GNU GPL and BSD licences.  *
 *   Detailed licence texts may be found in the COPYING file in the        *
 *   distribution tarball. The tool is a product of the ParaDiSe           *
 *   Laboratory, Faculty of Informatics of Masaryk University.             *
 *                                                                         *
 *   This distribution includes freely redistributable third-party code.   *
 *   Please refer to AUTHORS and COPYING included in the distribution for  *
 *   copyright and licensing details.                                      *
 ***************************************************************************/

#ifndef BASE_SHARED_EXPORT_H_
#define BASE_SHARED_EXPORT_H_

#ifdef BASE_SHARED_LIB
# define BASE_SHARED_EXPORT Q_DECL_EXPORT
#else
# define BASE_SHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // BASE_SHARED_EXPORT_H_

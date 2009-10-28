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

#ifndef PRINT_H_
#define PRINT_H_

#include "base_shared_export.h"

#include <QVariant>
#include <QString>

BASE_SHARED_EXPORT const QString quoteString(const QString & src);
BASE_SHARED_EXPORT const QString unquoteString(const QString & src);

BASE_SHARED_EXPORT const QString prettyPrint(const QVariantList & array);
BASE_SHARED_EXPORT const QString prettyPrint(const QVariantMap & map);
BASE_SHARED_EXPORT const QString prettyPrint(const QVariant & var);

#endif
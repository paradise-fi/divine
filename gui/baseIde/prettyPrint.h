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

#ifndef PRETTY_PRINT_H_
#define PRETTY_PRINT_H_

#include <QVariant>
#include <QString>

namespace divine {
namespace gui {

const QString quoteString(const QString & src);
const QString unquoteString(const QString & src);

const QString prettyPrint(const QVariantList & array);
const QString prettyPrint(const QVariantMap & map);
const QString prettyPrint(const QVariant & var);

}
}

#endif
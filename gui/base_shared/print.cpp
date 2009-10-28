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

#include <QVariant>
#include <QString>

#include "print.h"

const QString quoteString(const QString & src)
{
  QString res(src);

  res.replace("\"", "\\\"");

  res.prepend("\"");
  res.append("\"");
  return src;
}

const QString unquoteString(const QString & src)
{
  QString res(src);

  res.remove(0, 1);
  res.remove(res.size() - 1, 1);

  res.replace("\\\"", "\"");
  return src;
}

const QString prettyPrint(const QVariantMap & map)
{
  QString res;
  res.append("{");

  QVariantMap::const_iterator itr = map.begin();
  res.append(QString("%1=%2").arg(itr.key(), prettyPrint(itr.value())));

  for (++itr; itr != map.end(); ++itr) {
    res.append(QString("; %1=%2").arg(itr.key(), prettyPrint(itr.value())));
  }

  res.append("}");
  return res;
}

const QString prettyPrint(const QVariantList & array)
{
  QString res;
  res.append("{");

  for (int i = 0; i < array.size(); ++i) {
    res.append(prettyPrint(array.at(i)));

    if (i < array.size() - 1)
      res.append(",");
  }

  res.append("}");
  return res;
}

const QString prettyPrint(const QVariant & var)
{
  if (var.type() == QVariant::List) {
    return prettyPrint(var.toList());
  } else if (var.type() == QVariant::Map) {
    return prettyPrint(var.toMap());
  } else if (var.type() == QVariant::String) {
    return quoteString(var.toString());
  } else {
    return var.toString();
  }
}

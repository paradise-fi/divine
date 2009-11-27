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

#include <QApplication>
#include <QFont>

#include "settings.h"

// editor settings
const uint defEditorTabWidth = 4;
const bool defEditorNoTabs = false;
const bool defEditorAutoIndent = true;

// simulator settings
const bool defSimulatorRandom = true;
const uint defSimulatorSeed = 0;
const uint defSimulatorDelay = 0;
const uint defSimulatorSteps = 10;

const QColor defSimulatorNormal = QColor("#a5bfdc");
const QColor defSimulatorUsed = QColor("#5289e3");
const QColor defSimulatorHighlight = QColor("#ee8");

QSettings & sSettings(void)
{
  static QSettings set("ParaDiSe", "DiVinE IDE");
  return set;
}

const QFont defaultFont(const QString & family)
{
  QFont res = QApplication::font();
  res.setFamily(family);
  return res;
}

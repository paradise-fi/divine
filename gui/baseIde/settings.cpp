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
#include <QWidget>
#include <QFont>

#include "settings.h"
#include "abstractDocument.h"

namespace divine {
namespace gui {
  
namespace {

  const char * organization = "ParaDiSe";
  const char * application = "DiVinE IDE";

}

Settings::Settings(const QString & group)
  : QSettings(organization, application)
{
  Q_ASSERT(!group.isEmpty());

  beginGroup(group);
}

Settings::Settings(const QWidget * widget)
  : QSettings(organization, application)
{
  Q_ASSERT(widget);
  Q_ASSERT(!widget->objectName().isEmpty());

  beginGroup("widgets/" + widget->objectName());
}

Settings::Settings(const AbstractDocument * doc)
  : QSettings(organization, application)
{
  Q_ASSERT(doc);

  beginGroup("documents/" + doc->path());
}

Settings::~Settings()
{
  endGroup();
}
  
// editor settings
const uint defEditorTabWidth = 4;
const bool defEditorConvertTabs = false;
const bool defEditorAutoIndent = true;

// simulator settings
const bool defSimulatorRandom = true;
const uint defSimulatorSeed = 0;
const uint defSimulatorDelay = 0;
const uint defSimulatorSteps = 10;

const QString defVerificationAlgorithm = "OWCTY";
const uint defVerificationThreads = 2;
const uint defVerificationMaxMemory = 512;

const QColor defSimulatorNormal = QColor("#a5bfdc");
const QColor defSimulatorUsed = QColor("#5289e3");
const QColor defSimulatorHighlight = QColor("#ee8");

const QFont defaultFont(const QString & family)
{
  QFont res = QApplication::font();
  res.setFamily(family);
  return res;
}

}
}

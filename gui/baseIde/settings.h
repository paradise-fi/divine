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

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <QSettings>
#include <QFont>
#include <QColor>

namespace divine {
namespace gui {

class AbstractDocument;

class Settings : public QSettings
{ 
  public:
    Settings(const QString & group);
    Settings(const QWidget * widget);
    Settings(const AbstractDocument * doc);
    ~Settings();
};

// Gets default font from the application instance and initializes it.
// Storing default fonts in constants would result in incorrectly determined
// size, since font settings is initialized with the application instance.
const QFont defaultFont(const QString & family);

// default settings
inline const QFont defEditorFont() {return defaultFont("monospace");}

extern const uint defEditorTabWidth;
extern const bool defEditorConvertTabs;
extern const bool defEditorAutoIndent;

extern const bool defSimulatorRandom;
extern const uint defSimulatorSeed;
extern const uint defSimulatorDelay;
extern const uint defSimulatorSteps;

extern const QString defVerificationAlgorithm;
extern const uint defVerificationThreads;

// not configurable ATM
extern const QColor defSimulatorNormal;
extern const QColor defSimulatorUsed;
extern const QColor defSimulatorHighlight;

}
}

#endif

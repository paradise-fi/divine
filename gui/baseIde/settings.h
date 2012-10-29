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

#include "baseIdeExport.h"

#include <QSettings>
#include <QFont>
#include <QColor>

namespace divine {
namespace gui {

class AbstractDocument;

class BASE_IDE_EXPORT Settings : public QSettings
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
BASE_IDE_EXPORT const QFont defaultFont(const QString & family);

// default settings
inline const QFont defEditorFont() {return defaultFont("monospace");}

extern BASE_IDE_EXPORT const uint defEditorTabWidth;
extern BASE_IDE_EXPORT const bool defEditorConvertTabs;
extern BASE_IDE_EXPORT const bool defEditorAutoIndent;

extern BASE_IDE_EXPORT const bool defSimulatorRandom;
extern BASE_IDE_EXPORT const uint defSimulatorSeed;
extern BASE_IDE_EXPORT const uint defSimulatorDelay;
extern BASE_IDE_EXPORT const uint defSimulatorSteps;

extern BASE_IDE_EXPORT const QString defVerificationAlgorithm;
extern BASE_IDE_EXPORT const uint defVerificationThreads;

// not configurable ATM
extern BASE_IDE_EXPORT const QColor defSimulatorNormal;
extern BASE_IDE_EXPORT const QColor defSimulatorUsed;
extern BASE_IDE_EXPORT const QColor defSimulatorHighlight;

}
}

#endif

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

#include "base_shared_export.h"

#include <QSettings>
#include <QFont>
#include <QColor>

BASE_SHARED_EXPORT QSettings & sSettings(void);

// Gets default font from the application instance and initializes it.
// Storing default fonts in constants would result in incorrectly determined
// size, since font settings is initialized with the application instance.
BASE_SHARED_EXPORT const QFont defaultFont(const QString & family);

// default settings
inline const QFont defEditorFont(void) {return defaultFont("monospace");}

extern BASE_SHARED_EXPORT const uint defEditorTabWidth;
extern BASE_SHARED_EXPORT const bool defEditorNoTabs;
extern BASE_SHARED_EXPORT const bool defEditorAutoIndent;

extern BASE_SHARED_EXPORT const bool defSimulatorRandom;
extern BASE_SHARED_EXPORT const uint defSimulatorSeed;
extern BASE_SHARED_EXPORT const uint defSimulatorDelay;
extern BASE_SHARED_EXPORT const uint defSimulatorSteps;

extern BASE_SHARED_EXPORT const QColor defSimulatorNormal;
extern BASE_SHARED_EXPORT const QColor defSimulatorUsed;
extern BASE_SHARED_EXPORT const QColor defSimulatorHighlight;

#endif

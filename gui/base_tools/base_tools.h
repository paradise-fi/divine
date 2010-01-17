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

#ifndef BASE_TOOLS_H_
#define BASE_TOOLS_H_

#include <QColor>
#include <QFont>

#include "plugins.h"
#include "settings.h"

// default settings

// output window
inline const QFont defOutputFont(void) {return defaultFont("monospace");}

extern const bool defOutputEFont;
extern const bool defOutputSysColors;
extern const QColor defOutputFore;
extern const QColor defOutputBack;

// trace
extern const bool defTraceVars;
extern const bool defTraceVarNames;
extern const bool defTraceProcs;
extern const bool defTraceProcNames;
extern const bool defTraceBufs;
extern const bool defTraceBufNames;
extern const QColor defTraceDeadlock;
extern const QColor defTraceError;

/*!
 * The BaseToolsPlugin class is the main class in base_tools plugin.
 */
class BaseToolsPlugin : public QObject, public AbstractPlugin
{
    Q_OBJECT
    Q_INTERFACES(AbstractPlugin)

  public:
    void install(MainForm * root);
};

#endif

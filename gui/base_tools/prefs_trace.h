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

#ifndef PREFS_STACK_H_
#define PREFS_STACK_H_

#include "plugins.h"

namespace Ui {
  class TracePreferences;
}

/*!
 * This class implements the preferences page for the stack trace window.
 */
class TracePreferences : public PreferencesPage {
  Q_OBJECT

  public:
    TracePreferences(QWidget * parent=NULL);
    ~TracePreferences();

  public slots:
    void readSettings(void);
    void writeSettings(void);

  private:
    Ui::TracePreferences * ui_;

  private slots:
    void on_variableBox_stateChanged(int state);
    void on_processBox_stateChanged(int state);
    void on_channelBox_stateChanged(int state);
};

#endif

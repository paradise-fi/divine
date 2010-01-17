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

#ifndef PREFS_OUTPUT_H_
#define PREFS_OUTPUT_H_

#include "plugins.h"

namespace Ui {
  class OutputPreferences;
}

/*!
 * This class implements preferences page for the output window.
 */
class OutputPreferences : public PreferencesPage {
  Q_OBJECT

  public:
    OutputPreferences(QWidget * parent=NULL);
    ~OutputPreferences();

  public slots:
    void readSettings(void);
    void writeSettings(void);

  private:
    Ui::OutputPreferences * ui_;

  private slots:
    void on_editorBox_stateChanged(int state);
    void on_syscolBox_stateChanged(int state);
};

#endif

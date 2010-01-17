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

#ifndef PREFS_SIMULATOR_H_
#define PREFS_SIMULATOR_H_

#include "plugins.h"

namespace Ui
{

  class SimulatorPreferences;
}

/*!
 * This class implements preferences page for generic simulator.
 */
class SimulatorPreferences : public PreferencesPage
{
    Q_OBJECT

  public:
    SimulatorPreferences(QWidget * parent=NULL);
    ~SimulatorPreferences();

  public slots:
    void readSettings(void);
    void writeSettings(void);

  private:
    Ui::SimulatorPreferences * ui_;

  private slots:
    void on_randomBox_stateChanged(int state);
};

#endif

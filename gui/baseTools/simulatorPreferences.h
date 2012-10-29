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

#ifndef SIMULATOR_PREFERENCES_H_
#define SIMULATOR_PREFERENCES_H_

#include <QWidget>

namespace Ui
{
  class SimulatorPreferences;
}

namespace divine {
namespace gui {

/*!
 * This class implements preferences page for generic simulator.
 */
class SimulatorPreferences : public QWidget
{
    Q_OBJECT

  public:
    SimulatorPreferences(QWidget * parent=NULL);
    ~SimulatorPreferences();

  public slots:
    void readSettings();
    void writeSettings();

  private:
    Ui::SimulatorPreferences * ui_;

  private slots:
    void on_randomBox_stateChanged(int state);
};

}
}

#endif

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

#ifndef TRACE_PREFERENCES_H_
#define TRACE_PREFERENCES_H_

#include <QWidget>

namespace Ui {
  class TracePreferences;
}

namespace divine {
namespace gui {

/*!
 * This class implements the preferences page for the stack trace window.
 */
class TracePreferences : public QWidget {
  Q_OBJECT

  public:
    TracePreferences(QWidget * parent=NULL);
    ~TracePreferences();

  public slots:
    void readSettings();
    void writeSettings();

  private:
    Ui::TracePreferences * ui_;

  private slots:
    void on_variableBox_stateChanged(int state);
    void on_processBox_stateChanged(int state);
    void on_channelBox_stateChanged(int state);
};

}
}

#endif

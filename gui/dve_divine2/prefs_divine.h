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

#ifndef PREFS_MC_H_
#define PREFS_MC_H_

#include "plugins.h"

namespace Ui
{

  class DivinePreferences;
}

/*!
 * This class implements the preferences page for the DiVinE tools.
 */
class DivinePreferences : public PreferencesPage
{
    Q_OBJECT

  public:
    DivinePreferences(QWidget * parent=NULL);
    ~DivinePreferences();

  public slots:
    void readSettings(void);
    void writeSettings(void);

  private:
    Ui::DivinePreferences * ui_;

  private slots:
    void on_pathButton_clicked(void);
};

#endif

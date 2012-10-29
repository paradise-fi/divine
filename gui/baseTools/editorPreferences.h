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

#ifndef EDITOR_PREFERENCES_H_
#define EDITOR_PREFERENCES_H_

#include <QWidget>

namespace Ui {
  class EditorPreferences;
}

namespace divine {
namespace gui {

/*!
 * The EditorPreferences class implements preferences page for text editors.
 */
class EditorPreferences : public QWidget {
  Q_OBJECT
  
  public:
    EditorPreferences(QWidget * parent=NULL);
    ~EditorPreferences();

  public slots:
    void readSettings();
    void writeSettings();

  private:
    Ui::EditorPreferences * ui_;
};

}
}

#endif

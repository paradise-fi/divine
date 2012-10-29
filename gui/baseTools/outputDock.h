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

#ifndef OUTPUT_DOCK_H_
#define OUTPUT_DOCK_H_

#include <QDockWidget>
#include <QPoint>

class QString;
class QTextEdit;
class QAction;

namespace divine {
namespace gui {

/*!
 * The OutputDock class implements the output window.
 */
class OutputDock : public QDockWidget {
  Q_OBJECT

  public:
    OutputDock(QWidget * parent=NULL);

  public slots:
    void readSettings();
    void clear();
    void appendText(const QString & text);

  private:
    QTextEdit * edit_;
    QAction * clearAct_;
    
  private slots:
    void onCustomContextMenuRequested(const QPoint & pos);
};

}
}

#endif

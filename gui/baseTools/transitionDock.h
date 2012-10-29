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

#ifndef TRANSITION_DOCK_H_
#define TRANSITION_DOCK_H_

#include <QDockWidget>

class QTreeWidget;
class QTreeWidgetItem;

namespace divine {
namespace gui {

class MainForm;
class SimulationProxy;

/*!
 * This class implements the list of active transitions.
 */
class TransitionDock : public QDockWidget
{
    Q_OBJECT

  public:
    TransitionDock(MainForm * root);

  public slots:
    void updateTransitions();
    
  signals:
    //! Requests that specified transition should be highlighted.
    void highlightTransition(int id);
    void activateTransition(int id);
    
  protected:
    void leaveEvent(QEvent * event);
    
  private:
    SimulationProxy * sim_;
    QTreeWidget * tree_;

  private slots:
    void onStateChanged(int index);

    void onCurrentItemChanged(QTreeWidgetItem * current, QTreeWidgetItem *);
    void onItemActivated(QTreeWidgetItem * item, int);
    void onItemEntered(QTreeWidgetItem * item, int);
    void onViewportEntered();
};

}
}

#endif

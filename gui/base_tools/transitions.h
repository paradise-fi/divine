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

#ifndef TRANSITIONS_H_
#define TRANSITIONS_H_

#include <QDockWidget>

class SimulatorProxy;
class QTreeWidget;
class QTreeWidgetItem;

/*!
 * This class implements the list of active transitions.
 */
class TransitionDock : public QDockWidget
{
    Q_OBJECT

  public:
    TransitionDock(QWidget * parent = NULL);

  public slots:
    void setSimulator(SimulatorProxy * sim);

  signals:
    //! Requests that specified transition should be highlighted.
    void highlightTransition(int id);
    
  protected:
    void leaveEvent(QEvent * event);
    
  private:
    SimulatorProxy * sim_;
    
    QTreeWidget * tree_;

  private slots:
    void updateTransitions(void);

    void onCurrentItemChanged(QTreeWidgetItem * current, QTreeWidgetItem *);
    void onItemActivated(QTreeWidgetItem * item, int);
    void onItemEntered(QTreeWidgetItem * item, int);
    
};

#endif

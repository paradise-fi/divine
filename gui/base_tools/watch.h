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

#ifndef WATCH_H_
#define WATCH_H_

#include <QDockWidget>
#include <QVariant>

class QTreeWidget;
class QTreeWidgetItem;

class SimulatorProxy;

/*!
 * This class implements the watch window for variables.
 */
class WatchDock : public QDockWidget
{
    Q_OBJECT

  public:
    WatchDock(QWidget * parent = NULL);

  public slots:
    void setReadOnly(bool state);
    void setSimulator(SimulatorProxy * sim);

  private:
    QTreeWidget * tree_;
    SimulatorProxy * sim_;

  private:
    void updateChannels(QTreeWidgetItem * group, bool editable);
    void updateProcess(QTreeWidgetItem * group, int pid, bool editable);
    void updateArray(QTreeWidgetItem * item, const QVariantList & array, bool editable);
    void updateMap(QTreeWidgetItem * item, const QVariantMap & map, bool editable);
    void updateScalar(QTreeWidgetItem * item, const QVariant & value, bool editable);
    
    const QVariant packVariable(QTreeWidgetItem * item);
    
  private slots:
    void resetWatch(void);
    void updateWatch(void);
    
    void onItemChanged(QTreeWidgetItem * item, int column);
};

#endif

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

#ifndef WATCH_DOCK_H_
#define WATCH_DOCK_H_

#include <QDockWidget>
#include <QList>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;

namespace divine {
namespace gui {

class MainForm;
class SimulationProxy;

struct Symbol;
struct OptionType;
struct ArrayType;
struct PODType;
struct ListType;

/*!
 * This class implements the watch window for variables.
 */
class WatchDock : public QDockWidget
{
    Q_OBJECT

  public:
    WatchDock(MainForm * root);

  public slots:
    void updateWatch();

  private:
    QTreeWidget * tree_;
    SimulationProxy * sim_;
    
  private:
    void updateSymbol(QTreeWidgetItem * item, const Symbol * symbol);
    void updatePODItem(QTreeWidgetItem * item, const QVariant & value,
                      const PODType * type, bool readOnly);
    void updateOptionItem(QTreeWidgetItem * item, const QVariant & value,
                         const OptionType * type, bool readOnly);
    void updateArrayItem(QTreeWidgetItem * item, const QVariant & value,
                        const ArrayType * type, bool readOnly);
    void updateListItem(QTreeWidgetItem * item, const QVariant & value,
                       const ListType * type);
    void updateChildren(QTreeWidgetItem * parent, QStringList children);
    void updateListCaption(QTreeWidgetItem * parent);
    QVariant packItem(QTreeWidgetItem * item);
    
  private slots:
    void onStateChanged(int index);
    void onItemChanged(QTreeWidgetItem * item, int column);
};

}
}

#endif

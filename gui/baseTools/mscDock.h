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

#ifndef MSC_DOCK_H_
#define MSC_DOCK_H_

#include <QDockWidget>
#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QAbstractItemDelegate>

class QHeaderView;
class QPaintEvent;
class QMouseEvent;

namespace divine {
namespace gui {

class MainForm;
class SimulationProxy;

/*!
 * This class provides model of the simulated run for the model-view
 * architecture.
 *
 * This model very closely follows the desired widget behaviour:
 * <ul>
 *  <li>The Qt::DisplayRole returns the name of the changed state (or
 *      an empty string if the state has not changed).</li>
 *  <li>The SequenceModel::stateRole returns the state name in every case (for
 *      selected or "hovered" rows).</li>
 *  <li>The SequenceModel::messageRole gets information about synchronisation
 *      in the current row.</li>
 *  <li>The SequenceModel::currentStateRole simply states whether given row is
 *      also the current state in the simulation.</li>
 *  <li>The SequenceModel::enumStatesRole is used for getting names of process
 *      state values.</li>
 * </ul>
 */
class MSCModel : public QAbstractItemModel
{
    Q_OBJECT

  public:
    //! Returns states for every item unlike the DisplayRole.
    static const int stateRole = Qt::UserRole;
    //! Tells whether given row is also the current state.
    static const int currentStateRole = Qt::UserRole + 1;
    //! Returns names of states in the process.
    static const int enumStatesRole = Qt::UserRole + 2;
    //! Get information about synchronisation in the current row.
    static const int messageRole = Qt::UserRole + 3;
    static const int messageFromRole = Qt::UserRole + 4;
    static const int messageToRole = Qt::UserRole + 5;

  public:
    MSCModel(SimulationProxy * sim, QObject * parent = NULL);

    int rowCount(const QModelIndex & parent = QModelIndex()) const;
    int columnCount(const QModelIndex & parent = QModelIndex()) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex & index) const;
    
  private:
    struct ProcessInfo {
      QString id;
      QStringList states;
      int origin;
    };
    
  private:
    SimulationProxy * sim_;

    QList<ProcessInfo> procs_;
    
    int current_;
    
  private slots:
    void onReset();
    void onCurrentIndexChanged(int index);
    
    void onStateChanged(int index);
    void onStateAdded(int from, int to);
    void onStateRemoved(int from, int to);
    
  private:
    QVariant getMessage(int state) const;
    QVariant getMessageSender(int state) const;
    QVariant getMessageRecipient(int state) const;
};

/*!
 * This class does the painting for the sequence chart.
 */
class MSCDelegate : public QAbstractItemDelegate
{
  public:
    MSCDelegate(QObject * parent = NULL);
    
    void paint(QPainter * painter, const QStyleOptionViewItem & option,
               const QModelIndex & index) const;
    
    QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const;
};

/*!
 * This class implements the main widget of the sequence chart.
 * It is based on the Qt's model-view architecture.
 */
class MSCWidget : public QAbstractItemView
{
    Q_OBJECT

  public:
    MSCWidget(QWidget * parent = NULL);

    QModelIndex indexAt(const QPoint & point) const;
    void scrollTo(const QModelIndex & index, ScrollHint hint = EnsureVisible);
    int sizeHintForColumn(int column) const;
    QRect visualRect(const QModelIndex & index) const;

    void resizeColumnToContents(int column);
    
    void setModel(QAbstractItemModel * model);
    
  protected:
    int horizontalOffset() const;
    int verticalOffset() const;
    
    bool isIndexHidden(const QModelIndex & index) const;
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers);
    void scrollContentsBy(int dx, int dy);
    void setSelection(const QRect & rect, QItemSelectionModel::SelectionFlags flags);
    QRegion visualRegionForSelection(const QItemSelection & selection) const;
    
    void leaveEvent(QEvent * event);
    void mouseMoveEvent(QMouseEvent * event);
    void paintEvent(QPaintEvent * event);
    
  protected slots:
    void updateGeometries();
    
  private:
    QHeaderView * header_;
    
    int hover_;
    
  private:
    int rowHeight() const;
    
  private slots:
    void resetColumns();
    void updateHorizontalScrollBar();
    void updateVerticalScrollBar();
    
    void onColumnMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void onColumnResized(int logicalIndex, int oldSize, int newSize);
};

/*!
 * This class implements the sequence chart window.
 */
class MSCDock : public QDockWidget
{
    Q_OBJECT

  public:
    MSCDock(MainForm * root);

  private:
    MSCWidget * msc_;
    MSCModel * model_;
    SimulationProxy * sim_;
    
  private slots:
    void onItemActivated(const QModelIndex & index);
};

}
}

#endif

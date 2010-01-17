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

#ifndef SEQUENCE_H_
#define SEQUENCE_H_

#include <QDockWidget>
#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QAbstractItemDelegate>

class QHeaderView;
class QPaintEvent;
class QMouseEvent;

class Simulator;
class SimulatorProxy;

/*!
 * This class provides model of the simulated run for the model-view
 * architecture.
 *
 * This model very closely follows the desired sequence chart:
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
class SequenceModel : public QAbstractItemModel
{
    Q_OBJECT

  public:
    //! Returns states for every item unlike the DisplayRole.
    static const int stateRole = Qt::UserRole;
    //! Get information about synchronisation in the current row.
    static const int messageRole = Qt::UserRole + 1;
    //! Tells whether given row is also the current state.
    static const int currentStateRole = Qt::UserRole + 2;
    //! Returns names of states in the process.
    static const int enumStatesRole = Qt::UserRole + 3;

  public:
    SequenceModel(const SimulatorProxy * sim, QObject * parent = NULL);

    int rowCount(const QModelIndex & parent = QModelIndex()) const;
    int columnCount(const QModelIndex & parent = QModelIndex()) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex & index) const;
    
  public slots:
    void resetModel(void);
    void updateModel(void);
    
  private:
    const Simulator * sim_;

    int state_;
    int depth_;
    
    QList<QStringList> states_;
};

/*!
 * This class does the painting for the sequence chart.
 */
class SequenceDelegate : public QAbstractItemDelegate
{
  public:
    SequenceDelegate(QObject * parent = NULL);
    
    void paint(QPainter * painter, const QStyleOptionViewItem & option,
               const QModelIndex & index) const;
    
    QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const;
};

/*!
 * This class implements the main widget of the sequence chart.
 * It is based on the Qt's model-view architecture.
 */
class SequenceWidget : public QAbstractItemView
{
    Q_OBJECT

  public:
    SequenceWidget(QWidget * parent = NULL);

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
    void updateGeometries(void);
    
  private:
    QHeaderView * header_;
    
    int hover_;
    
  private:
    int rowHeight(void) const;
    
  private slots:
    void resetColumns(void);
    void updateHorizontalScrollBar(void);
    void updateVerticalScrollBar(void);
    
    void onColumnMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void onColumnResized(int logicalIndex, int oldSize, int newSize);
};

/*!
 * This class implements the sequence chart window.
 */
class SequenceDock : public QDockWidget
{
    Q_OBJECT

  public:
    SequenceDock(QWidget * parent = NULL);

  public slots:
    void setSimulator(SimulatorProxy * sim);

  private:
    SequenceWidget * msc_;
    SequenceModel * model_;
    SimulatorProxy * sim_;
    
  private slots:
    void onItemActivated(const QModelIndex & index);
};

#endif

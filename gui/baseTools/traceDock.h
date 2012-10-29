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

#ifndef TRACE_DOCK_H_
#define TRACE_DOCK_H_

#include <QDockWidget>
#include <QListWidget>
#include <QColor>

#include "lineNumberBar.h"

class QListWidgetItem;

namespace divine {
namespace gui {

class SimulationProxy;
class CycleBar;
class LineNumberBar;
class MainForm;

struct Symbol;
struct PODType;
struct OptionType;
struct ArrayType;
struct ListType;

/*!
 * This class implements the main widget of the stack trace window.
 */
class TraceWidget : public QListWidget, public LineNumberBarHost
{
    Q_OBJECT

  public:
    TraceWidget(QWidget * parent);

    const QPair<int, int> & acceptingCycle() const;

    // side bar host
    int lineCount();
    int firstVisibleLine(const QRect & rect);
    QList<int> visibleLines(const QRect & rect);

  public slots:
    void setAcceptingCycle(const QPair<int, int> & cycle);
    void updateMargins(int width);

  signals:
    void lineCountChanged();

  protected:
    void resizeEvent(QResizeEvent * event);
    void scrollContentsBy(int dx, int dy);

  private:
    CycleBar * cbar_;
    LineNumberBar * lbar_;
};

/*!
 * This class implements the stack trace window.
 */
class TraceDock : public QDockWidget
{
    Q_OBJECT

  public:
    TraceDock(MainForm * root);

  public slots:
    void readSettings();

    void reset();
    void setCurrentState(int index);

    void updateState(int index);
    
    void addStates(int from, int to);
    void removeStates(int from, int to);
    
  signals:
    void stateActivated(int index);
    
  private:
    TraceWidget * trace_;
    SimulationProxy * sim_;
    
    int state_;
    
    QColor deadlock_;
    QColor error_;

  private:
    QString printSymbol(const Symbol * symbol, int state);
    QString printPODItem(const QVariant & value, const PODType * type);
    QString printOptionItem(const QVariant & value, const OptionType * type);
    QString printArrayItem(const QVariant & value, const ArrayType * type, int state);
    QString printChildren(const QStringList & children, int state);
    
  private slots:
    void onItemActivated(QListWidgetItem * item);
};

}
}

#endif

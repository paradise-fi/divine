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

#ifndef STACK_H_
#define STACK_H_

#include <QDockWidget>
#include <QListWidget>
#include <QColor>

class QListWidgetItem;
class SimulatorProxy;
class CycleBar;
class PreferencesPage;

/*!
 * This class implements the main widget of the stack trace window.
 */
class TraceWidget : public QListWidget
{
    Q_OBJECT

    friend class CycleBar;

  public:
    TraceWidget(QWidget * parent = NULL);

    //! Gets the current accepting cycle.
    const QPair<int, int> & acceptingCycle(void) const {return cycle_;}
    void setAcceptingCycle(const QPair<int, int> & cycle);

  protected:
    void resizeEvent(QResizeEvent * event);
    void scrollContentsBy(int dx, int dy);
    
  private:
    CycleBar * cbar_;

    QPair<int, int> cycle_;

  private:
    void sideBarPaintEvent(QPaintEvent * event);
};

/*!
 * This class implements the stack trace window.
 */
class TraceDock : public QDockWidget
{
    Q_OBJECT

  public:
    TraceDock(QWidget * parent = NULL);

  public slots:
    void readSettings(void);
    void setSimulator(SimulatorProxy * sim);

  private:
    TraceWidget * trace_;
    SimulatorProxy * sim_;
    
    int state_;

    // settings
    bool variable_;
    bool variableName_;
    bool process_;
    bool processName_;
    bool channel_;
    bool channelName_;
    
    QColor deadlock_;
    QColor error_;

  private:
    void updateTraceItem(int state);
    
  private slots:
    void onSimulatorStarted(void);
    void onSimulatorStateChanged(void);
    void onItemActivated(QListWidgetItem * item);
};

#endif

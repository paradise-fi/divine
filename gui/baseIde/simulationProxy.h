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

#ifndef SIMULATION_PROXY_H_
#define SIMULATION_PROXY_H_

#include <QObject>
#include <QVariant>

#include "baseIdeExport.h"
#include "abstractSimulator.h"

class QAction;

namespace divine {
namespace gui {

class MainForm;
class SimulationLock;
class SimulationLoader;

class BASE_IDE_EXPORT SimulationProxy : public QObject
{
    Q_OBJECT

  public:
    SimulationProxy(MainForm * parent = NULL);
    ~SimulationProxy();

    bool isRunning() const;
    bool isReadOnly() const {return readOnly_;}

    // current state on the stack
    int currentIndex() const;

    QPair<int, int> acceptingCycle() const {return cycle_;}

    // simulator access for model-specific needs
    AbstractSimulator * simulator() {return simulator_;}

    // proxy functions
    int stateCount() const;

    bool isAccepting(int state);
    bool isValid(int state);

    // transitions
    int transitionCount(int state = -1);
    const Transition * transition(int index, int state = -1);
    QList<const Communication*> communication(int index, int state = -1);
    QList<const Transition*> transitions(int state = -1);

    int usedTransition(int state = -1);

    // symbol table
    const SymbolType * typeInfo(const QString & id) const;

    // state access
    const Symbol * symbol(const QString & id, int state = -1);
    QList<QString> topLevelSymbols(int state = -1);
    QVariant symbolValue(const QString & id, int state = -1);
    bool setSymbolValue(const QString & id, const QVariant & value, int state = -1);

    // auto-loader
    void autoRun(const QVector<int> & steps, int delay = 0);

  public slots:
    void start();
    void stop();
    void restart();

    void setCurrentIndex(int index);

    void next();
    void previous();
    void step(int id);
    void random();

    void updateActions();

  signals:
    void started();
    void reset();
    void stopped();

    void currentIndexChanged(int index);
    void acceptingCycleChanged(const QPair<int, int> & cycle);

    void stateChanged(int index);
    void stateAdded(int from, int to);
    void stateRemoved(int from, int to);

    void highlightTransition(int index);

  private slots:
    void randomRun();
    void importRun();
    void exportRun();

    void autoStep(int id = -1);
    void autoFinished();

  private:
    MainForm * root_;
    AbstractSimulator * simulator_;
    SimulationLock * lock_;
    SimulationLoader * loader_;

    QPair<int, int> cycle_;
    int index_;

    // actions
    QAction * startAct_;
    QAction * stopAct_;
    QAction * restartAct_;
    QAction * nextAct_;
    QAction * prevAct_;
    QAction * randomAct_;
    QAction * randomRunAct_;
    QAction * importAct_;
    QAction * exportAct_;

    // auto-run stuff
    bool readOnly_;

  private:
    bool isValidIndex(int state) const;

    void createActions();
    void createMenus();
    void createToolbars();
};

}
}

#endif

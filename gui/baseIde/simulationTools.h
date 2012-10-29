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

#ifndef SIMULATION_TOOLS_H_
#define SIMULATION_TOOLS_H_

#include <atomic>

#include <QThread>
#include <QVector>

#include "abstractToolLock.h"

namespace divine {
namespace gui {

class MainForm;

class SimulationLock : public QObject, public AbstractToolLock
{
  Q_OBJECT

  public:
    SimulationLock(MainForm * root, QObject * parent);
    ~SimulationLock();

    // public interface
    bool isLocked(AbstractDocument * document) const;
    void lock(AbstractDocument * document);

    bool maybeRelease();
    void forceRelease();

    // local functions used from SimulationProxy
    AbstractDocument * document() { return doc_; }

  private:
    MainForm * root_;
    AbstractDocument * doc_;
};

class SimulationLoader : public QThread
{
  Q_OBJECT

  public:
    SimulationLoader(QObject * parent);

    void run();

    void init(const QVector<int>& steps, int delay);

  public slots:
    void abort();

  signals:
    void step(int id);
    void progress(int value);

  private:
    QVector<int> steps_;
    int current_;
    int delay_;

    std::atomic<bool> running_;
};

}
}

#endif

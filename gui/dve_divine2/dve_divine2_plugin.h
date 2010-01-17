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

#ifndef DVE_DIVINE2_PLUGIN_H_
#define DVE_DIVINE2_PLUGIN_H_

#include "plugins.h"

#include <QProcess>

class QAction;
class QActionGroup;

// default values
const int defDivThreads = 2;
const int defDivMemory = 0;
const bool defDivNoPool = false;
const bool defDivReport = false;
const char defDivPath[] = "divine";

const int defDivAlgorithm = 0;

//! Builds the DVE simulator.
class DveSimulatorFactory : public SimulatorFactory
{
    Q_OBJECT
  public:
    DveSimulatorFactory(QObject * parent) : SimulatorFactory(parent) {}

    Simulator * load(const QString & fileName, MainForm * root);
};

/*!
 * This class is the main class of the dve_divine2 plugin. It provides
 * connection to the divine-2 tool and implements front-ends for most
 * of it's tools.
 */
class DvePlugin : public QObject, public AbstractPlugin
{
    Q_OBJECT
    Q_INTERFACES(AbstractPlugin)

  public:
    DvePlugin() : divineProcess_(NULL) {}
    
    void install(MainForm * root);
    
  signals:
    //! Sends a text message to the output window.
    void message(const QString & msg);
    
  private:
    QAction * combineAct_;
    QAction * reachabilityAct_;
    QAction * metricsAct_;
    QAction * verifyAct_;
    QAction * abortAct_;
    QActionGroup * algorithmGroup_;
    
    // divine runner
    QProcess * divineProcess_;
    QString divineFile_;
    qint16 divineCRC_;
    
    MainForm * root_;
    
  private:
    void prepareDivine(QStringList & args);
    void runDivine(const QString & algorithm);
    
  private slots:
    void combine(void);
    
    void reachability(void);
    void metrics(void);
    void verify(void);
    
    void abort(void);
    
    void onRunnerError(QProcess::ProcessError error);
    void onRunnerFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onEditorChanged(SourceEditor * editor);
    void onAlgorithmTriggered(QAction * action);
};

#endif

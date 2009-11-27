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

class DveSimulatorLoader : public SimulatorLoader
{
    Q_OBJECT
  public:
    DveSimulatorLoader(QObject * parent) : SimulatorLoader(parent) {}

    Simulator * load(const QString & fileName, MainForm * root);
};

// plugin class
class DvePlugin : public QObject, public AbstractPlugin
{
    Q_OBJECT
    Q_INTERFACES(AbstractPlugin)

  public:
    DvePlugin() : divineProcess_(NULL) {}
    
    void install(MainForm * root);
    
  signals:
    void message(const QString & msg);
    
  private:
    QAction * combineAct_;
    QAction * reachabilityAct_;
    QAction * metricsAct_;
    QAction * verifyAct_;
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
    
    void onRunnerError(QProcess::ProcessError error);
    void onRunnerFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onEditorChanged(SourceEditor * editor);
    void onAlgorithmTriggered(QAction * action);
};

#endif

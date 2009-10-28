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
class QMenu;

// default values
const int defDivThreads = 2;
const int defDivMemory = 0;
const bool defDivNoPool = false;
const bool defDivReport = false;
const char defDivPath[] = "divine";

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
    QAction * syntaxAct_;
    QAction * combineAct_;
    QMenu * algorithmMenu_;
    
    // divine runner
    QProcess * divineProcess_;
    QString divineFile_;
    qint16 divineCRC_;
    
    MainForm * root_;
    
  private:
    void prepareDivine(QStringList & args);
    void runDivine(const QString & algorithm);
    
  private slots:
    void checkSyntax(void);
    void combine(void);
    void runAlgorithm(QAction * action);
    
    void onRunnerError(QProcess::ProcessError error);
    void onRunnerFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onEditorChanged(SourceEditor * editor);
};

#endif

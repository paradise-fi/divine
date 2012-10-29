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

#ifndef DVE_PLUGIN_H_
#define DVE_PLUGIN_H_

#include "modules.h"

class QAction;
class QMenu;

namespace divine {

struct Meta;

namespace gui {

class AbstractEditor;
class DivineLock;
class DivineRunner;
class MainForm;

class DveDocumentFactory : public AbstractDocumentFactory
{
  public:
    DveDocumentFactory(QObject * parent) : AbstractDocumentFactory(parent) {}

    QString name() const;
    QString suffix() const;
    QIcon icon() const;
    QString filter() const;

    AbstractDocument * create(MainForm * root) const;
};

class MDveDocumentFactory : public AbstractDocumentFactory
{
  public:
    MDveDocumentFactory(QObject * parent) : AbstractDocumentFactory(parent) {}

    QString name() const;
    QString suffix() const;
    QIcon icon() const;
    QString filter() const;

    AbstractDocument * create(MainForm * root) const;
};

class LtlDocumentFactory : public AbstractDocumentFactory
{
  public:
    LtlDocumentFactory(QObject * parent) : AbstractDocumentFactory(parent) {}

    QString name() const;
    QString suffix() const;
    QIcon icon() const;
    QString filter() const;

    AbstractDocument * create(MainForm * root) const;
};

class DveSimulatorFactory : public AbstractSimulatorFactory
{
  public:
    DveSimulatorFactory(QObject* parent) : AbstractSimulatorFactory(parent) {}

    QString suffix() const;

    AbstractSimulator * create(MainForm * root) const;
};

class DveModule : public QObject, public AbstractModule
{
    Q_OBJECT

  public:
    DveModule();

    void install(MainForm * root);

  private:
    QAction * syntaxAct_;
    QAction * preprocessorAct_;
    QAction * combineAct_;
    QAction * dveSep_;

    MainForm * root_;

    DivineLock * lock_;
    DivineRunner * runner_;

  signals:
    void message(const QString & msg);

  private slots:
    void updateActions();

    void onEditorDeactivated();

    void onSyntaxTriggered();
    void onPreprocessorTriggered();
    void onCombineTriggered();
    void onMetricsTriggered();
    void onReachabilityTriggered();
    void onSearchTriggered();
    void onVerifyTriggered();

    void onRunnerFinished();

  private:
    void runAlgorithm(const Meta & meta);
};

}
}

#endif

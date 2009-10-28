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

#ifndef PLUGINS_H_
#define PLUGINS_H_

#include <QWidget>

class MainForm;
class SourceEditor;
class Simulator;

class EditorBuilder : public QObject
{
    Q_OBJECT

  public:
    EditorBuilder(QObject * parent = NULL) : QObject(parent) {}

    virtual void install(SourceEditor * editor) = 0;
    virtual void uninstall(SourceEditor * editor) = 0;
};

class SimulatorLoader : public QObject
{
    Q_OBJECT

  public:
    SimulatorLoader(QObject * parent = NULL) : QObject(parent) {}

    virtual Simulator * load(const QString & fileName, MainForm * root) = 0;
};

class PreferencesPage : public QWidget
{
    Q_OBJECT

  public:
    PreferencesPage(QWidget * parent) : QWidget(parent) {}

  public slots:
    virtual void readSettings() = 0;
    virtual void writeSettings() = 0;
};

class AbstractPlugin
{
  public:
    virtual ~AbstractPlugin() {};

    virtual void install(MainForm * root) = 0;
};

Q_DECLARE_INTERFACE(AbstractPlugin, "edu.ParaDiSe.DiVinE_IDE.AbstractPlugin/1.0")

#endif

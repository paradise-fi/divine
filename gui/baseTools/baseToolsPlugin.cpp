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

#include <QtPlugin>
#include <QAction>
#include <QMenu>

#include "baseToolsPlugin.h"
#include "mainForm.h"
#include "simulationProxy.h"
#include "pluginManager.h"
#include "preferencesDialog.h"
#include "layoutManager.h"

#include "outputPreferences.h"
#include "editorPreferences.h"
#include "simulatorPreferences.h"
#include "tracePreferences.h"

#include "filesystem.h"
#include "outputDock.h"
#include "traceDock.h"
#include "watchDock.h"
#include "transitionDock.h"
#include "mscDock.h"

#include "textDocument.h"
#include "textEditor.h"
#include "textEditorHandlers.h"

namespace divine {
namespace gui {

// output
const bool defOutputEFont = true;
const bool defOutputSysColors = true;
const QColor defOutputFore = QColor("#000");
const QColor defOutputBack = QColor("#fff");

// trace
const bool defTraceVars = true;
const bool defTraceVarNames = false;
const bool defTraceProcs = true;
const bool defTraceProcNames = true;
const bool defTraceBufs = true;
const bool defTraceBufNames = false;
const QColor defTraceDeadlock = QColor("#88e");
const QColor defTraceError = QColor("#d55");

//
// Text editor factory
//
QString TextDocumentFactory::name() const
{
  return "Text file";
}

QString TextDocumentFactory::suffix() const
{
  return "";
}

QIcon TextDocumentFactory::icon() const
{
  return MainForm::loadIcon(":/icons/filetypes/generic");
}

QString TextDocumentFactory::filter() const
{
  return "All files";
}

AbstractDocument * TextDocumentFactory::create(MainForm * root) const
{
  TextDocument * document = new TextDocument(root);
  TextEditor * editor = qobject_cast<TextEditor*>(document->editor(document->mainView()));
  Q_ASSERT(editor);
  
  // basic handlers
  SimpleIndenter * indenter = new SimpleIndenter(editor);
  connect(root, SIGNAL(settingsChanged()), indenter, SLOT(readSettings()));
  editor->addKeyEventHandler(indenter);

  SimpleTabConverter * converter = new SimpleTabConverter(editor);
  connect(root, SIGNAL(settingsChanged()), converter, SLOT(readSettings()));
  editor->addKeyEventHandler(converter);

  return document;
}

//
// Plugin implementation
//
void BaseToolsPlugin::install(MainForm * root)
{
  // preferences
  QWidget * page = new OutputPreferences();
  root->preferences()->addWidget(QObject::tr("IDE"), QObject::tr("Output"), page);

  page = new EditorPreferences();
  root->preferences()->addWidget(QObject::tr("IDE"), QObject::tr("Editor"), page);

  page = new SimulatorPreferences();
  root->preferences()->addWidget(QObject::tr("IDE"), QObject::tr("Simulator"), page);

//   page = new TracePreferences();
//   root->preferences()->addWidget(QObject::tr("IDE"), QObject::tr("Trace"), page);

  // text editor
  TextDocumentFactory * fab = new TextDocumentFactory(this);
  root->plugins()->registerDocument(fab);

  // panels
  QMenu * menu = root->findChild<QMenu*>("main.menu.view.panels");
  QAction * action;
  Q_ASSERT(menu);
  
  // file system browser
  FileSystemBrowserDock * fs = new FileSystemBrowserDock(root);
  fs->setObjectName("common.dock.filesystem");
  root->addDockWidget(Qt::LeftDockWidgetArea, fs);

  action = fs->toggleViewAction();
  action->setText(QObject::tr("&Filesystem"));
  action->setStatusTip(QObject::tr("Toggles the filesystem panel"));
  menu->addAction(action);
  
  // output
  OutputDock * output = new OutputDock(root);
  output->setObjectName("common.dock.output");
  root->addDockWidget(Qt::BottomDockWidgetArea, output);

  connect(root, SIGNAL(message(const QString &)), output, SLOT(appendText(const QString &)));
  connect(root, SIGNAL(settingsChanged()), output, SLOT(readSettings()));

  action = output->toggleViewAction();
  action->setText(QObject::tr("&Output"));
  action->setStatusTip(QObject::tr("Toggles the output panel"));
  menu->addAction(action);

  // watch
  WatchDock * watch = new WatchDock(root);
  watch->setObjectName("common.dock.watch");
  root->addDockWidget(Qt::LeftDockWidgetArea, watch);

  action = watch->toggleViewAction();
  action->setText(QObject::tr("&Watch"));
  action->setStatusTip(QObject::tr("Toggles the watch panel"));
  menu->addAction(action);

  root->layouts()->addWidget(watch, QStringList("debug"));
  root->layouts()->addAction(action, QStringList("debug"));

  // trace
  TraceDock * stack = new TraceDock(root);
  stack->setObjectName("common.dock.stack");
  root->addDockWidget(Qt::BottomDockWidgetArea, stack);

  action = stack->toggleViewAction();
  action->setText(QObject::tr("&Stack trace"));
  action->setStatusTip(QObject::tr("Toggles the stack trace panel"));
  menu->addAction(action);

  root->layouts()->addWidget(stack, QStringList("debug"));
  root->layouts()->addAction(action, QStringList("debug"));

  // transitions
  TransitionDock * trans = new TransitionDock(root);
  trans->setObjectName("common.dock.transitions");
  root->addDockWidget(Qt::RightDockWidgetArea, trans);

  action = trans->toggleViewAction();
  action->setText(QObject::tr("&Enabled transitions"));
  action->setStatusTip(QObject::tr("Toggles the enabled transitions panel"));
  menu->addAction(action);

  root->layouts()->addWidget(trans, QStringList("debug"));
  root->layouts()->addAction(action, QStringList("debug"));

  // message sequence chart
  MSCDock * msc = new MSCDock(root);
  msc->setObjectName("common.dock.msc");
  root->addDockWidget(Qt::RightDockWidgetArea, msc);

  action = msc->toggleViewAction();
  action->setText(QObject::tr("&Message Sequence Chart"));
  action->setStatusTip(QObject::tr("Toggles the sequence diagram panel"));
  menu->addAction(action);

  root->layouts()->addWidget(msc, QStringList("debug"));
  root->layouts()->addAction(action, QStringList("debug"));
}

}
}

Q_EXPORT_PLUGIN2(baseTools, divine::gui::BaseToolsPlugin)

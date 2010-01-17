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
#include <QCompleter>
#include <QUrl>

#include "dve_editor_plugin.h"
#include "dve_highlighter.h"
#include "prefs_dve.h"
#include "mainform.h"
#include "editor.h"

DveEditorBuilder::DveEditorBuilder(MainForm * root) : EditorBuilder(root), root_(root)
{
}

void DveEditorBuilder::install(SourceEditor * editor)
{
  QUrl url(editor->document()->metaInformation(QTextDocument::DocumentUrl));
  
  DveHighlighter * highlight = new DveHighlighter(url.scheme() == "mdve", editor->document());
  highlight->setObjectName("dveHighlighter");
  
  connect(root_, SIGNAL(settingsChanged()), highlight, SLOT(readSettings()));
  
  QStringList wordList;
  wordList << "accept" << "and" << "assert" << "async" << "byte" << "channel"
           << "commit" << "const" << "effect" << "false" << "guard" << "int"
           << "imply" << "init" << "or" << "process" << "property" << "state"
           << "sync" << "system" << "trans" << "true";

  QCompleter * completer = new QCompleter(wordList, editor);
  editor->setCompleter(completer);
}

void DveEditorBuilder::uninstall(SourceEditor * editor)
{
  DveHighlighter * highlight = editor->document()->findChild<DveHighlighter*>("dveHighlighter");
  Q_ASSERT(highlight);
  delete highlight;
  
  editor->setCompleter(NULL);
}

void DveEditorPlugin::install(MainForm * root)
{
  // document loaders
  EditorBuilder * loader = new DveEditorBuilder(root);
  root->registerDocument("dve", tr("DVE files (*.dve)"), loader);
  root->registerDocument("mdve", tr("MDVE files (*.mdve)"), loader);
  
  // preferences
  PreferencesPage * page = new DvePreferences();
  root->registerPreferences(QObject::tr("Syntax"), QObject::tr("DVE"), page);
}

Q_EXPORT_PLUGIN2(dve_editor, DveEditorPlugin)

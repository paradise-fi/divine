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
#include <QUrl>

#include "ltl_editor_plugin.h"
#include "ltl_highlighter.h"
#include "prefs_ltl.h"
#include "mainform.h"
#include "editor.h"

LtlEditorBuilder::LtlEditorBuilder(MainForm * root) : EditorBuilder(root), root_(root)
{
}

void LtlEditorBuilder::install(SourceEditor * editor)
{
  QUrl url(editor->document()->metaInformation(QTextDocument::DocumentUrl));
  
  LtlHighlighter * highlight = new LtlHighlighter(editor->document());
  highlight->setObjectName("ltlHighlighter");
  
  connect(root_, SIGNAL(settingsChanged()), highlight, SLOT(readSettings()));                                                                             
}

void LtlEditorBuilder::uninstall(SourceEditor * editor)
{
  LtlHighlighter * highlight = editor->document()->findChild<LtlHighlighter*>("ltlHighlighter");
  Q_ASSERT(highlight);
  delete highlight;
}

void LtlEditorPlugin::install(MainForm * root)
{
  // document loaders
  EditorBuilder * loader = new LtlEditorBuilder(root);
  root->registerDocument("ltl", tr("LTL files (*.ltl)"), loader);
  
  // preferences
  PreferencesPage * page = new LtlPreferences();
  root->registerPreferences(QObject::tr("Syntax"), QObject::tr("LTL"), page);
}

Q_EXPORT_PLUGIN2(ltl_editor, LtlEditorPlugin)

/***************************************************************************
 *   Copyright (C) 2012 by Martin Moracek                                  *
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

#include <QFileInfo>

#include "mainForm.h"
#include "textEditor.h"
#include "textDocument.h"

namespace divine {
namespace gui {

TextDocument::TextDocument(MainForm * root) : AbstractDocument(root), root_(root)
{
  editor_ = new TextEditor(root, this);
}

TextDocument::~TextDocument()
{
}

QString TextDocument::path() const
{
  return editor_->sourcePath();
}

int TextDocument::viewCount() const
{
  return 1;
}

QStringList TextDocument::views() const
{
  return QStringList(mainView());
}

QString TextDocument::mainView() const
{
  return editor_->viewName();
}

AbstractEditor * TextDocument::editor(const QString & view)
{
  if(view != mainView())
    return NULL;
  
  return editor_;
}

void TextDocument::openView(const QString & view)
{
  if(view != mainView())
    return;
  
  root_->addEditor(editor_);
}

bool TextDocument::openFile(const QString & path)
{
  return editor_->openFile(path);
}

}
}

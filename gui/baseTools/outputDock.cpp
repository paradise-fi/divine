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

#include <QTextEdit>
#include <QString>
#include <QFont>
#include <QMenu>

#include "baseToolsModule.h"
#include "settings.h"
#include "outputDock.h"

namespace divine {
namespace gui {

// editor settings
static const uint maxOutputLines = 0;

OutputDock::OutputDock(QWidget * parent)
    : QDockWidget(tr("Output"), parent)
{
  clearAct_ = new QAction(tr("&Clear"), this);
  clearAct_->setObjectName("common.action.clearOutput");
  connect(clearAct_, SIGNAL(triggered(bool)), SLOT(clear()));
  
  edit_ = new QTextEdit(this);
  edit_->setReadOnly(true);
  edit_->document()->setMaximumBlockCount(maxOutputLines);
  edit_->setContextMenuPolicy(Qt::CustomContextMenu);
  
  connect(edit_, SIGNAL(customContextMenuRequested(QPoint)), SLOT(onCustomContextMenuRequested(QPoint)));
  
  setWidget(edit_);
  
  readSettings();
}

//! Reloads settings.
void OutputDock::readSettings()
{
  Settings s("ide/output");

  bool useEFont = s.value("useEditorFont", defOutputEFont).toBool();
  bool useSysCols = s.value("useSystemColors", defOutputSysColors).toBool();

  // if clauses
  if (!useEFont)
    edit_->setFont(s.value("font", defOutputFont()).value<QFont>());

  QPalette pal;

  if (!useSysCols) {
    QColor fore = s.value("background", defOutputFore).value<QColor>();
    QColor back = s.value("foreground", defOutputFore).value<QColor>();

    pal = edit_->palette();
    pal.setColor(QPalette::Text, fore);
    pal.setColor(QPalette::Base, back);
  }

  edit_->setPalette(pal);

  // fallback font
  if (useEFont) {
    edit_->setFont(Settings("ide/editor").value("font", defEditorFont()).value<QFont>());
  }
}

//! Clears the output window.
void OutputDock::clear()
{
  edit_->clear();
}

//! Appends given text to the output window.
void OutputDock::appendText(const QString & text)
{
  edit_->append(text);

  QTextCursor c = edit_->textCursor();
  c.movePosition(QTextCursor::End);
  edit_->setTextCursor(c);
}

void OutputDock::onCustomContextMenuRequested(const QPoint & pos)
{
  QMenu * menu = edit_->createStandardContextMenu();
  menu->addSeparator();
  menu->addAction(clearAct_);
  
  menu->exec(mapToGlobal(pos));
}

}
}

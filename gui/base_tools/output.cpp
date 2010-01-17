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

#include "base_tools.h"
#include "settings.h"
#include "output.h"

// editor settings
static const uint maxOutputLines = 0;

OutputDock::OutputDock(QWidget * parent)
    : QDockWidget(tr("Output"), parent)
{
  edit_ = new QTextEdit(this);
  edit_->setReadOnly(true);
  edit_->document()->setMaximumBlockCount(maxOutputLines);

  setWidget(edit_);

  readSettings();
}

//! Reloads settings.
void OutputDock::readSettings(void)
{
  QSettings & s = sSettings();

  s.beginGroup("Output");

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

  s.endGroup();

  // fallback font

  if (useEFont) {
    s.beginGroup("Editor");
    edit_->setFont(s.value("font", defEditorFont()).value<QFont>());
    s.endGroup();
  }
}

//! Clears the output window.
void OutputDock::clear(void)
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

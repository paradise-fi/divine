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
#include "console.h"

// editor settings
static const uint maxConsoleLines = 0;

ConsoleDock::ConsoleDock(QWidget * parent)
    : QDockWidget(tr("Log console"), parent)
{
  edit_ = new QTextEdit(this);
  edit_->setReadOnly(true);
  edit_->document()->setMaximumBlockCount(maxConsoleLines);

  setWidget(edit_);

  readSettings();
}

bool ConsoleDock::isEmpty(void) const
{
  return edit_->document()->isEmpty();
}

void ConsoleDock::readSettings(void)
{
  QSettings & s = sSettings();

  s.beginGroup("Console");

  bool useEFont = s.value("useEditorFont", defConsoleEFont).toBool();
  bool useSysCols = s.value("useSystemColours", defConsoleSysColours).toBool();

  // if clauses

  if (!useEFont)
    edit_->setFont(s.value("font", defConsoleFont()).value<QFont>());

  QPalette pal;

  if (!useSysCols) {
    QColor fore = s.value("background", defConsoleFore).value<QColor>();
    QColor back = s.value("foreground", defConsoleFore).value<QColor>();

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

void ConsoleDock::clear(void)
{
  edit_->clear();
}

void ConsoleDock::appendText(const QString & text)
{
  edit_->append(text);

  QTextCursor c = edit_->textCursor();
  c.movePosition(QTextCursor::End);
  edit_->setTextCursor(c);
}

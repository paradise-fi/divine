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

#include <QAbstractItemView>
#include <QScrollBar>
#include <QTextBlock>

#include "settings.h"
#include "textEditorHandlers.h"

namespace divine {
namespace gui {

namespace {
  const char * s_eow = "~!@#$%^&*()_+{}|:\"<>?,./;'[]\\-="; // end of word
}
  
//
// SimpleIndenter
//
SimpleIndenter::SimpleIndenter(TextEditor * editor)
  : QObject(editor), editor_(editor)
{
  readSettings();
}

bool SimpleIndenter::handleKeyPressEvent
(QKeyEvent * event, const QRect * cursorRect)
{
  Q_ASSERT(editor_);

  if(!enabled_)
    return false;
  
  // handle line breaks
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    QString text = editor_->textCursor().block().text();
    uint fill = 0;

    while (text[fill].isSpace() && static_cast<int>(fill) < text.size())
      ++fill;

    // copy the padding to the next line
    editor_->textCursor().insertText("\n" + text.left(fill));
    return true;
  }
  return false;
}

void SimpleIndenter::readSettings()
{
  enabled_ = Settings("ide/editor").value("autoIndent", defEditorAutoIndent).toBool();
}

//
// SimpleTabConverter
//
SimpleTabConverter::SimpleTabConverter(TextEditor * editor)
  : QObject(editor), editor_(editor)
{
  readSettings();
}

bool SimpleTabConverter::handleKeyPressEvent
(QKeyEvent * event, const QRect * cursorRect)
{
  Q_ASSERT(editor_);

  if(!enabled_)
    return false;
  
  // convert tabs to spaces
  if (event->key() == Qt::Key_Tab) {
    const uint col = editor_->textCursor().columnNumber();
    const uint fill = (col / tabWidth_ + 1) * tabWidth_ - col;

    editor_->textCursor().insertText(QString(fill, ' '));
    return true;
  }
  return false;
}

void SimpleTabConverter::readSettings()
{
  Settings s("ide/editor");

  enabled_ = s.value("convertTabs", defEditorConvertTabs).toBool();
  tabWidth_ = s.value("tabWidth", defEditorTabWidth).toUInt();

  if (tabWidth_ == 0)
    tabWidth_ = 1;
}

//
// SimpleCompleter
//
SimpleCompleter::SimpleCompleter(const QStringList& list, TextEditor * editor)
  : QCompleter(list, editor), editor_(editor)
{
  setWidget(editor->findChild<QWidget*>("plainTextEdit+"));
  setCompletionMode(QCompleter::PopupCompletion);
  setCaseSensitivity(Qt::CaseSensitive);
  setModelSorting(CaseSensitivelySortedModel);

  connect(this, SIGNAL(activated(const QString &)),
          this, SLOT(insertCompletion(const QString &)));
}

// TODO: moving the cursor out of the current word doesn't close the popup
bool SimpleCompleter::handleKeyPressEvent
(QKeyEvent * event, const QRect * cursorRect)
{
  Q_ASSERT(editor_);
  Q_ASSERT(cursorRect);

  QString eow(s_eow); // end of word
  
  if (popup()->isVisible()) {
    // The following keys are forwarded by the completer to the widget
    switch (event->key()) {
      case Qt::Key_Enter:
      case Qt::Key_Return:
      case Qt::Key_Escape:
      case Qt::Key_Tab:
      case Qt::Key_Backtab:
        event->ignore();
        return true; // let the completer do default behavior
      default:
        // close the popup if we hit an invalid character
        if (!event->text().isEmpty() && eow.contains(event->text()))
          popup()->hide();
        break;
    }
  }

  // Ctrl+Space combo
  const bool isCtrlSpace = event->modifiers() == Qt::ControlModifier &&
                          event->key() == Qt::Key_Space;

  if (isCtrlSpace || popup()->isVisible()) {
    setCompletionPrefix(getCompletionPrefix());
    popup()->setCurrentIndex(completionModel()->index(0, 0));

    QAbstractItemModel * mdl = completionModel();

    if (mdl->rowCount() == 1) {
      insertCompletion(mdl->itemData(mdl->index(0, 0))[Qt::DisplayRole].toString());
      popup()->hide();
      return true;
    } else {
      QRect rect = *cursorRect;
      rect.setWidth(popup()->sizeHintForColumn(0) +
                    popup()->verticalScrollBar()->sizeHint().width());

      // update popup!
      complete(rect);
    }
  }
  return false;
}

void SimpleCompleter::selectWordUnderCursor(QTextCursor & cursor) const
{
  QString eow(s_eow);

  int left, right;

  // left
  left = getCompletionPrefix().length();

  // right
  QTextCursor tc = cursor;
  tc.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
  QString str = tc.selectedText();

  for (right = 0; right < str.length(); ++right) {
    if (eow.contains(str[right]))
      break;
  }

  cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, left);
  cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, left + right);
}

QString SimpleCompleter::getCompletionPrefix() const
{
  // get current prefix
  QTextCursor tc = editor_->textCursor();

  // must be empty prefix
  if (tc.atBlockStart())
    return QString();

  tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
  tc.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);

  QString prefix = tc.selectedText();
  QString eow(s_eow);

  // check for \words and {} [] () etc.
  for (int i = prefix.length() - 1; i >= 0; --i) {
    if (eow.contains(prefix[i])) {
      prefix.remove(0, i + 1);
      break;
    }
  }
  return prefix;
}

void SimpleCompleter::insertCompletion(const QString & text)
{
  QTextCursor tc = editor_->textCursor();

  // replaces current word with completion
  selectWordUnderCursor(tc);

  tc.insertText(text);

  // doesn't erase current text
//   int extra = text.length() - completionPrefix().length();
//   tc.insertText(text.right(extra));
  editor_->setTextCursor(tc);
}

}
}

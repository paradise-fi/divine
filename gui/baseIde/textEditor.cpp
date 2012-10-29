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

#include <QLabel>
#include <QLayout>
#include <QBoxLayout>
#include <QPlainTextEdit>
#include <QFileInfo>
#include <QTextStream>
#include <QAction>
#include <QTextBlock>
#include <QUrl>
#include <QMessageBox>
#include <QStatusBar>

#include "mainForm.h"
#include "settings.h"
#include "textEditor.h"
#include "lineNumberBar.h"
#include "searchPanel.h"

namespace divine {
namespace gui {

//
// EnhancedPlainTextEdit
//
EnhancedPlainTextEdit::EnhancedPlainTextEdit(QWidget* parent)
  : QPlainTextEdit(parent), leftMargin_(0), extraEvents_(false)
{
  setObjectName("plainTextEdit+");

  selUnderCursor_ = -1;
}

int EnhancedPlainTextEdit::lineCount()
{
  return blockCount();
}

int EnhancedPlainTextEdit::firstVisibleLine(const QRect & rect)
{
  return firstVisibleBlock(rect).blockNumber();
}

QList<int> EnhancedPlainTextEdit::visibleLines(const QRect & rect)
{
  QList<int> res;

  QTextBlock block = firstVisibleBlock(rect);

  int top = blockBoundingGeometry(block).translated(contentOffset()).top();
  while(block.isValid() && top < rect.bottom()) {
    res.append(top);

    block = block.next();
    top = blockBoundingGeometry(block).translated(contentOffset()).top();
  }
  return res;
}

void EnhancedPlainTextEdit::addKeyEventHandler(AbstractKeyPressHandler * handler)
{
  handlers_.append(handler);
}

void EnhancedPlainTextEdit::setExtraSelections(const ExtraSelectionList & sels, bool reset)
{
  QList<QTextEdit::ExtraSelection> slist;

  selectionIds_.clear();

  foreach(ExtraSelection info, sels) {
    QTextEdit::ExtraSelection esel;
    esel.cursor = rectToCursor(info.from, info.to);
    esel.format = info.format;

    slist.append(esel);
    selectionIds_.append(info.id);
  }

  QPlainTextEdit::setExtraSelections(slist);
  checkSelOnPos(mapFromGlobal(QCursor::pos()) - QPoint(leftMargin_, 0), reset);
}

void EnhancedPlainTextEdit::requestExtraMouseEvents(bool state)
{
  extraEvents_ = state;

  setMouseTracking(extraEvents_);
  checkSelOnPos(mapFromGlobal(QCursor::pos()) - QPoint(leftMargin_, 0), true);
}

void EnhancedPlainTextEdit::updateMargins(int width)
{
  if (isLeftToRight()) {
    setViewportMargins(width, 0, 0, 0);
    leftMargin_ = width;
  } else {
    setViewportMargins(0, 0, width, 0);
  }
}

void EnhancedPlainTextEdit::dragEnterEvent(QDragEnterEvent * event)
{
  if (event->mimeData()->hasUrls())
    event->acceptProposedAction();
  else
    QPlainTextEdit::dragEnterEvent(event);
}

void EnhancedPlainTextEdit::dropEvent(QDropEvent * event)
{
  QString path = MainForm::getDropPath(event);

  if (!path.isEmpty()) {
    emit fileDropped(path);
  } else {
    QPlainTextEdit::dropEvent(event);
  }
}

void EnhancedPlainTextEdit::hideEvent(QHideEvent * event)
{
  // disable mouse over
  if(extraEvents_ && selUnderCursor_ != -1) {
    selUnderCursor_ = -1;
    emit selectionMouseOver(-1);
  }
}

void EnhancedPlainTextEdit::resizeEvent(QResizeEvent * event)
{
  QPlainTextEdit::resizeEvent(event);

  emit resized();
}

void EnhancedPlainTextEdit::keyPressEvent(QKeyEvent * event)
{
  QRect rect = cursorRect();

  foreach(AbstractKeyPressHandler * handler, handlers_) {
    if(handler->handleKeyPressEvent(event, &rect))
      return;
  }
  QPlainTextEdit::keyPressEvent(event);
}

void EnhancedPlainTextEdit::mouseDoubleClickEvent(QMouseEvent * event)
{
  int sel;
  if (!extraEvents_ || (sel = getSelectionFromPos(event->pos())) == -1) {
    QPlainTextEdit::mouseDoubleClickEvent(event);
  } else {
    Q_ASSERT(sel >= 0 && sel < selectionIds_.size());
    emit selectionDoubleClicked(selectionIds_[sel]);
  }
}

void EnhancedPlainTextEdit::mouseMoveEvent(QMouseEvent * event)
{
  QPlainTextEdit::mouseMoveEvent(event);

  checkSelOnPos(event->pos(), false);
}

QTextBlock EnhancedPlainTextEdit::firstVisibleBlock(const QRect & rect)
{
  QTextBlock block = QPlainTextEdit::firstVisibleBlock();

  while(block.previous().isValid() && blockBoundingGeometry(
        block).translated(contentOffset()).top() > rect.top()) {
    block = block.previous();
  }

  while(block.next().isValid() && blockBoundingGeometry(
        block).translated(contentOffset()).bottom() <= rect.top()) {
    block = block.previous();
  }
  return block;
}

int EnhancedPlainTextEdit::getSelectionFromPos(const QPoint& pos)
{
  QTextCursor cur = cursorForPosition(pos);

  // visibility order should last-to-first
  for (int i = extraSelections().size() - 1; i >= 0; --i) {
    const QTextEdit::ExtraSelection itr = extraSelections().at(i);

    if (cur.position() >= itr.cursor.selectionStart() &&
        cur.position() <= itr.cursor.selectionEnd()) {
      return i;
    }
  }
  return -1;
}

QTextCursor EnhancedPlainTextEdit::rectToCursor(const QPoint & from, const QPoint & to)
{
  QTextCursor res = textCursor();

  res.setPosition(0);
  res.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, from.y() - 1);
  res.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, from.x() - 1);

  res.setPosition(0, QTextCursor::KeepAnchor);
  res.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, to.y() - 1);
  res.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, to.x());
  return res;
}

void EnhancedPlainTextEdit::checkSelOnPos(const QPoint & pos, bool reset)
{
  if(!extraEvents_ || !isVisible())
    return;

  int sel = getSelectionFromPos(pos);
  int selId = -1;

  if(sel != -1) {
    Q_ASSERT(sel >= 0 && sel < selectionIds_.size());
    selId = selectionIds_.at(sel);
  }

  if(!reset && selId == selUnderCursor_)
    return;

  selUnderCursor_ = selId;
  emit selectionMouseOver(selUnderCursor_);
}

//
// Text editor
//
TextEditor::TextEditor(MainForm * root, AbstractDocument * doc)
  : AbstractEditor(root), root_(root), doc_(doc), active_(false)
{
  edit_ = new EnhancedPlainTextEdit(this);
  lineBar_ = new LineNumberBar(edit_, edit_);
  search_ = new SearchPanel(this);
  search_->setVisible(false);

  QBoxLayout * layout = new QBoxLayout(QBoxLayout::TopToBottom, this);
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(edit_);
  layout->addWidget(search_);

  connect(root_, SIGNAL(settingsChanged()), SLOT(readSettings()));
  connect(root_, SIGNAL(editorActivated(AbstractEditor*)), SLOT(onEditorActivated(AbstractEditor*)));
  connect(root_, SIGNAL(editorDeactivated()), SLOT(onEditorDeactivated()));
  connect(root_, SIGNAL(editorClosed(AbstractEditor*)), SLOT(onEditorClosed(AbstractEditor*)));

  connect(edit_->document(), SIGNAL(modificationChanged(bool)), SLOT(onModificationChanged(bool)));

  connect(edit_, SIGNAL(selectionDoubleClicked(int)), SIGNAL(selectionDoubleClicked(int)));
  connect(edit_, SIGNAL(selectionMouseOver(int)), SIGNAL(selectionMouseOver(int)));

  connect(edit_, SIGNAL(updateRequest(QRect,int)), SLOT(onUpdateRequest(QRect,int)));
  connect(edit_, SIGNAL(blockCountChanged(int)), lineBar_, SLOT(lineCountChanged()));
  connect(lineBar_, SIGNAL(widthChanged(int)), edit_, SLOT(updateMargins(int)));
  connect(edit_, SIGNAL(resized()), lineBar_, SLOT(updateGeometry()));

  connect(search_, SIGNAL(closed()), edit_, SLOT(setFocus()));
  connect(search_, SIGNAL(closed()), root_->statusBar(), SLOT(clearMessage()));
  connect(search_, SIGNAL(findNext()), SLOT(findNext()));
  connect(search_, SIGNAL(findPrevious()), SLOT(findPrevious()));
  connect(search_, SIGNAL(replace()), SLOT(replace()));
  connect(search_, SIGNAL(replaceAll()), SLOT(replaceAll()));

  connect(edit_, SIGNAL(fileDropped(QString)), root_, SLOT(openDocument(QString)));
}

TextEditor::~TextEditor()
{
}

bool TextEditor::isModified() const
{
  return edit_->document()->isModified();
}

QString TextEditor::viewName() const
{
  if(path_.isEmpty())
    return tr("untitled");
  else
    return QFileInfo(path_).fileName();
}

QString TextEditor::sourcePath() const
{
  return path_;
}

QTextDocument * TextEditor::textDocument()
{
  return edit_->document();
}

QTextCursor TextEditor::textCursor() const
{
  return edit_->textCursor();
}

void TextEditor::setTextCursor(const QTextCursor & cursor)
{
  edit_->setTextCursor(cursor);
}

void TextEditor::print(QPrinter * printer) const
{
  edit_->print(printer);
}

bool TextEditor::openFile(const QString & path)
{
  QFile file(path);

  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    QMessageBox::critical(
      this, tr("DiVinE IDE"),
      tr("Cannot read file %1:\n%2.").arg(path).arg(file.errorString()));
    return false;
  }

  // check for binary files
  QByteArray sample = file.read(1024);
  for(int i = 0; i < sample.size(); ++i) {
    // gotcha!
    if(sample[i] == '\x0') {
      int ret = QMessageBox::warning(this, tr("DiVinE IDE"),
              tr("The file appears to be binary.\nDo you still wish to open it?"),
              QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape);

      if(ret == QMessageBox::No)
        return false;
      break;
    }
  }
  file.reset();

  // good to go
  QTextStream in(&file);

  edit_->setPlainText(in.readAll());
  edit_->document()->setModified(false);

  path_ = path;
  return true;
}

bool TextEditor::saveFile(const QString & path)
{
  QFile file(path);

  if (!file.open(QFile::WriteOnly | QFile::Text)) {
    QMessageBox::critical(
      this, tr("DiVinE IDE"),
      tr("Cannot write file %1:\n%2.").arg(path).arg(file.errorString()));
    return false;
  }

  QTextStream out(&file);
  out << edit_->toPlainText();

  path_ = path;
  root_->updateTabLabel(this);

  edit_->document()->setModified(false);
  return true;
}

bool TextEditor::reload()
{
  return openFile(sourcePath());
}

void TextEditor::addKeyEventHandler(AbstractKeyPressHandler * handler)
{
  edit_->addKeyEventHandler(handler);
}

void TextEditor::setExtraSelections
(const EnhancedPlainTextEdit::ExtraSelectionList & sels, bool reset)
{
  edit_->setExtraSelections(sels, reset);
}

void TextEditor::requestExtraMouseEvents(bool state)
{
  edit_->requestExtraMouseEvents(state);
}

void TextEditor::readSettings()
{
  setFont(Settings("ide/editor").value("font", defEditorFont()).value<QFont>());
}

void TextEditor::updateActions()
{
  QAction * undoAct = root_->findChild<QAction*>("main.action.edit.undo");
  QAction * redoAct = root_->findChild<QAction*>("main.action.edit.redo");
  QAction * cutAct = root_->findChild<QAction*>("main.action.edit.cut");
  QAction * copyAct = root_->findChild<QAction*>("main.action.edit.copy");
  QAction * pasteAct = root_->findChild<QAction*>("main.action.edit.paste");
  QAction * overwriteAct = root_->findChild<QAction*>("main.action.edit.overwrite");
  QAction * findAct = root_->findChild<QAction*>("main.action.edit.find");
  QAction * findNextAct = root_->findChild<QAction*>("main.action.edit.findNext");
  QAction * findPrevAct = root_->findChild<QAction*>("main.action.edit.findPrevious");
  QAction * replaceAct = root_->findChild<QAction*>("main.action.edit.replace");
  QAction * wrapAct = root_->findChild<QAction*>("main.action.view.dynamicWordWrap");
  QAction * numbersAct = root_->findChild<QAction*>("main.action.view.lineNumbers");

  // enable actions
  undoAct->setEnabled(active_ && edit_->document()->isUndoAvailable() && !edit_->isReadOnly());
  redoAct->setEnabled(active_ && edit_->document()->isRedoAvailable() && !edit_->isReadOnly());
  cutAct->setEnabled(active_ && edit_->textCursor().hasSelection() && !edit_->isReadOnly());
  copyAct->setEnabled(active_ && edit_->textCursor().hasSelection());
  pasteAct->setEnabled(active_ && !edit_->isReadOnly());
  overwriteAct->setEnabled(active_);
  findAct->setEnabled(active_);
  findNextAct->setEnabled(active_ && !search_->pattern().isEmpty());
  findPrevAct->setEnabled(active_ && !search_->pattern().isEmpty());
  replaceAct->setEnabled(active_ && !edit_->isReadOnly());
  wrapAct->setEnabled(active_);
  numbersAct->setEnabled(active_);

  root_->findChild<QLabel*>("main.status.label")->setVisible(active_);

  // (dis)connect signalsedit_
  if(active_) {
    connect(undoAct, SIGNAL(triggered(bool)), edit_, SLOT(undo()));
    connect(redoAct, SIGNAL(triggered(bool)), edit_, SLOT(redo()));
    connect(cutAct, SIGNAL(triggered(bool)), edit_, SLOT(cut()));
    connect(copyAct, SIGNAL(triggered(bool)), edit_, SLOT(copy()));
    connect(pasteAct, SIGNAL(triggered(bool)), edit_, SLOT(paste()));

    connect(edit_, SIGNAL(undoAvailable(bool)), undoAct, SLOT(setEnabled(bool)));
    connect(edit_, SIGNAL(redoAvailable(bool)), redoAct, SLOT(setEnabled(bool)));
    connect(edit_, SIGNAL(copyAvailable(bool)), cutAct, SLOT(setEnabled(bool)));
    connect(edit_, SIGNAL(copyAvailable(bool)), copyAct, SLOT(setEnabled(bool)));

    connect(findAct, SIGNAL(triggered(bool)), SLOT(onFindTriggered()));
    connect(replaceAct, SIGNAL(triggered(bool)), SLOT(onReplaceTriggered()));
    connect(findNextAct, SIGNAL(triggered(bool)), search_, SIGNAL(findNext()));
    connect(findPrevAct, SIGNAL(triggered(bool)), search_, SIGNAL(findPrevious()));
    connect(search_, SIGNAL(searchAvailable(bool)), findNextAct, SLOT(setEnabled(bool)));
    connect(search_, SIGNAL(searchAvailable(bool)), findPrevAct, SLOT(setEnabled(bool)));

    overwriteAct->setChecked(edit_->overwriteMode());
    wrapAct->setChecked(edit_->lineWrapMode() != QPlainTextEdit::NoWrap);
    numbersAct->setChecked(lineBar_->isVisible());

    connect(overwriteAct, SIGNAL(toggled(bool)), SLOT(onOverwriteToggled(bool)));
    connect(wrapAct, SIGNAL(toggled(bool)), SLOT(onLineWrapToggled(bool)));
    connect(numbersAct, SIGNAL(toggled(bool)), lineBar_, SLOT(setVisible(bool)));

    connect(edit_, SIGNAL(cursorPositionChanged()), SLOT(updateCursorInfo()));

    updateCursorInfo();
  } else {
    disconnect(undoAct, SIGNAL(triggered(bool)), edit_, SLOT(undo()));
    disconnect(redoAct, SIGNAL(triggered(bool)), edit_, SLOT(redo()));
    disconnect(cutAct, SIGNAL(triggered(bool)), edit_, SLOT(cut()));
    disconnect(copyAct, SIGNAL(triggered(bool)), edit_, SLOT(copy()));
    disconnect(pasteAct, SIGNAL(triggered(bool)), edit_, SLOT(paste()));

    disconnect(edit_, SIGNAL(undoAvailable(bool)), undoAct, SLOT(setEnabled(bool)));
    disconnect(edit_, SIGNAL(redoAvailable(bool)), redoAct, SLOT(setEnabled(bool)));
    disconnect(edit_, SIGNAL(copyAvailable(bool)), cutAct, SLOT(setEnabled(bool)));
    disconnect(edit_, SIGNAL(copyAvailable(bool)), copyAct, SLOT(setEnabled(bool)));

    disconnect(findAct, SIGNAL(triggered(bool)), this, SLOT(onFindTriggered()));
    disconnect(replaceAct, SIGNAL(triggered(bool)), this, SLOT(onReplaceTriggered()));
    disconnect(findNextAct, SIGNAL(triggered(bool)), search_, SIGNAL(findNext()));
    disconnect(findPrevAct, SIGNAL(triggered(bool)), search_, SIGNAL(findPrevious()));
    disconnect(search_, SIGNAL(searchAvailable(bool)), findNextAct, SLOT(setEnabled(bool)));
    disconnect(search_, SIGNAL(searchAvailable(bool)), findPrevAct, SLOT(setEnabled(bool)));

    disconnect(overwriteAct, SIGNAL(toggled(bool)), this, SLOT(onOverwriteToggled(bool)));
    disconnect(wrapAct, SIGNAL(toggled(bool)), this, SLOT(onLineWrapToggled(bool)));
    disconnect(numbersAct, SIGNAL(toggled(bool)), lineBar_, SLOT(setVisible(bool)));

    disconnect(edit_, SIGNAL(cursorPositionChanged()), this, SLOT(updateCursorInfo()));
  }
}

void TextEditor::updateCursorInfo()
{
  QTextCursor cur = edit_->textCursor();

  root_->findChild<QLabel*>("main.status.label")->setText(
    tr("Line: %1 Col: %2").arg(QString::number(cur.blockNumber() + 1),
                               QString::number(cur.columnNumber() + 1)));
}

void TextEditor::resizeEvent(QResizeEvent * event)
{
  lineBar_->updateGeometry();
}

void TextEditor::onEditorActivated(AbstractEditor * editor)
{
  if(editor != this)
    return;

  active_ = true;
  updateActions();
}

void TextEditor::onEditorDeactivated()
{
  if(!active_)
    return;

  active_ = false;
  updateActions();
}

void TextEditor::onEditorClosed(AbstractEditor * editor)
{
  if(editor != this)
    return;

  doc_->deleteLater();
  this->deleteLater();
}

void TextEditor::onModificationChanged(bool state)
{
  emit modificationChanged(this, state);
}

void TextEditor::onOverwriteToggled(bool state)
{
  edit_->setOverwriteMode(state);
}

void TextEditor::onLineWrapToggled(bool state)
{
  edit_->setLineWrapMode(state ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
}

void TextEditor::onUpdateRequest(const QRect & r, int dy)
{
  if(dy)
    lineBar_->scroll(0, dy);

  if (r.contains(edit_->viewport()->rect()))
    lineBar_->update();
}

void TextEditor::onFindTriggered()
{
  showSearchPanel(false);
}

void TextEditor::onReplaceTriggered()
{
  showSearchPanel(true);
}

void TextEditor::showSearchPanel(bool replace)
{
  QString hint;
  if(edit_->textCursor().hasSelection()) {
    hint = edit_->textCursor().selectedText();
  } else {
    QTextCursor cur = edit_->textCursor();
    cur.select(QTextCursor::WordUnderCursor);
    hint = cur.selectedText();
  }

  search_->show(hint, replace);
}

void TextEditor::findNext()
{
  search(true);
}

void TextEditor::findPrevious()
{
  search(false);
}

void TextEditor::replace()
{
  // selection matches pattern
  if(search_->pattern().exactMatch(edit_->textCursor().selectedText()))
    edit_->textCursor().insertText(search_->sample());

  findNext();
}

void TextEditor::replaceAll()
{
  QTextDocument::FindFlags flags = 0;

  if(search_->modeWholeWords())
    flags |= QTextDocument::FindWholeWords;

  QTextCursor cur = edit_->textCursor();
  cur.movePosition(QTextCursor::Start);

  do {
    cur = edit_->document()->find(search_->pattern(), cur, flags);
    cur.insertText(search_->sample());
  } while (!cur.isNull());
}

void TextEditor::search(bool forward)
{
  QTextDocument::FindFlags flags = 0;

  if(!forward)
    flags |= QTextDocument::FindBackward;

  if(search_->modeWholeWords())
    flags |= QTextDocument::FindWholeWords;

  QTextCursor cur = edit_->textCursor();
  if(!search_->pattern().exactMatch(cur.selectedText())) {
    // move cursor to the beginning of the selection
    cur.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor,
                    cur.position() - cur.selectionStart());
    edit_->setTextCursor(cur);
  }

  // do the finding
  cur = edit_->document()->find(search_->pattern(), edit_->textCursor(), flags);

  // to prevent misleading messages
  root_->statusBar()->clearMessage();

  if(cur.isNull()) {
    cur = edit_->document()->find(search_->pattern(), 0, flags);

    if(cur.isNull())
      root_->statusBar()->showMessage(tr("No match"));
    else
      root_->statusBar()->showMessage(tr("Passed the end of file"));
  }

  if(!cur.isNull())
    edit_->setTextCursor(cur);
}

}
}

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

#include <limits.h>

#include <QKeyEvent>
#include <QCompleter>
#include <QSettings>
#include <QUrl>
#include <QTextBlock>
#include <QStyleOptionFrame>
#include <QScrollBar>
#include <QAbstractItemView>
#include <QPainter>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>

#include "editor.h"
#include "settings.h"
#include "simulator.h"

static const char * s_eow = "~!@#$%^&*()_+{}|:\"<>?,./;'[]\\-="; // end of word

/*!
 * The Sidebar class draws line numbers on the side of a text editor.
 * \see SourceEditor
 */

class SideBar : public QWidget
{

  public:
    SideBar(SourceEditor * parent);

    int calculateWidth(int lines);

  protected:
    void paintEvent(QPaintEvent * event);

  private:
    SourceEditor * edit_;
};

SideBar::SideBar(SourceEditor * parent) : QWidget(parent), edit_(parent)
{
  setAutoFillBackground(true);
}

//! Calculates width of the sid bar based on the max. number of digits.
int SideBar::calculateWidth(int lines)
{
  uint nums = QString::number(lines).length();

  if (nums < 2)
    nums = 2;

  return fontMetrics().width(QString(nums + 1, '0'))/* + 6*/;
}

//! Forwards painting to the SourceEditor class.
void SideBar::paintEvent(QPaintEvent * event)
{
  edit_->sideBarPaintEvent(event);
}

//
// SourceEdit class
//
SourceEditor::SourceEditor(QWidget * parent)
    : QPlainTextEdit(parent), completer_(NULL), numbar_(NULL), sim_(NULL)
{
  setMouseTracking(true);
  setAcceptDrops(true);
  setLineWrapMode(QPlainTextEdit::NoWrap);

  showLineNumbers_ = true;

  // setup number panel
  numbar_ = new SideBar(this);

  updateSideBarWidth(blockCount());

  readSettings();

  connect(document(), SIGNAL(blockCountChanged(int)), SLOT(updateSideBarWidth(int)));
  connect(document(), SIGNAL(modificationChanged(bool)), SLOT(onDocumentModified(bool)));

  connect(this, SIGNAL(updateRequest(const QRect &, int)), SLOT(onUpdateRequest(const QRect &, int)));
}

SourceEditor::~SourceEditor()
{
}

//! Assigns a new code completer.
void SourceEditor::setCompleter(QCompleter * cmpl)
{
  if (cmpl == completer_)
    return;

  if (completer_) {
    disconnect(completer_, 0, this, 0);
    delete completer_;
  }

  completer_ = cmpl;

  // done?

  if (!completer_)
    return;

  completer_->setWidget(this);

  completer_->setCompletionMode(QCompleter::PopupCompletion);

  completer_->setCaseSensitivity(Qt::CaseSensitive);

  connect(completer_, SIGNAL(activated(const QString &)),
          this, SLOT(insertCompletion(const QString &)));
}

/*!
 * Shows the side bar.
 * \see SideBar
 */
void SourceEditor::setShowLineNumbers(bool show)
{
  showLineNumbers_ = show;

  updateSideBarWidth(blockCount());
}

/*!
 * Highlights provided block of code with given colour.
 * \param block Code block. QRect specifies upper left and lower right character.
 * \param color Desired color.
 */
void SourceEditor::highlightBlock(const QRect & block, const QColor & color)
{
  QTextEdit::ExtraSelection esel;

  esel.cursor = rectToCursor(block);
  esel.format.setBackground(color);

  blocks_.append(esel);

  updateExtraSelections();
}

//! Clears all user-specified highlighting.
void SourceEditor::resetHighlighting(void)
{
  blocks_.clear();

  updateExtraSelections();
}

//! Loads contents from a given file.
bool SourceEditor::loadFile(const QString & path)
{
  QFile file(path);

  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    QMessageBox::warning(
      this, tr("DiVinE IDE"),
      tr("Cannot read file %1:\n%2.").arg(path).arg(file.errorString()));
    return false;
  }

  QTextStream in(&file);

  setPlainText(in.readAll());

  document()->setModified(false);
  return true;
}

//! Saves contents to a given file.
bool SourceEditor::saveFile(const QString & path)
{
  QFile file(path);

  if (!file.open(QFile::WriteOnly | QFile::Text)) {
    QMessageBox::warning(
      this, tr("DiVinE IDE"),
      tr("Cannot write file %1:\n%2.").arg(path).arg(file.errorString()));
    return false;
  }

  QTextStream out(&file);

  out << toPlainText();

  document()->setModified(false);
  return true;
}

//! Reloads settings.
void SourceEditor::readSettings(void)
{
  QSettings & s = sSettings();

  s.beginGroup("Editor");

  tabWidth_ = s.value("tabWidth", defEditorTabWidth).toUInt();

  if (tabWidth_ == 0)
    tabWidth_ = 1;

  noTabs_ = s.value("tabWidth", defEditorNoTabs).toBool();
  autoIndent_ = s.value("autoIndent", defEditorAutoIndent).toBool();

  setFont(s.value("font", defEditorFont()).value<QFont>());
  setTabStopWidth(fontMetrics().width(QString(tabWidth_, ' ')));

  s.endGroup();

  // transition colours
  s.beginGroup("Simulator");

  normal_ = s.value("normal", defSimulatorNormal).value<QColor>();
  lastUsed_ = s.value("used", defSimulatorUsed).value<QColor>();
  hovered_ = s.value("highlight", defSimulatorHighlight).value<QColor>();

  s.endGroup();

  updateExtraSelections();
}

//! Enables automatic highlighting of transition from given simulator.
void SourceEditor::autoHighlight(SimulatorProxy * sim)
{
  if (sim_ == sim)
    return;

  activeTrans_ = -1;

  if (sim_) {
    disconnect(sim_, SIGNAL(started()), this, SLOT(onSimulatorUpdate()));
    disconnect(sim_, SIGNAL(stateChanged()), this, SLOT(onSimulatorUpdate()));
  }

  sim_ = sim;

  if (sim_) {
    connect(sim_, SIGNAL(started()), this, SLOT(onSimulatorUpdate()));
    connect(sim_, SIGNAL(stateChanged()), this, SLOT(onSimulatorUpdate()));
  }

  updateExtraSelections();
}

//! Highlights specified transition. Auto highlighting must be turned on.
void SourceEditor::highlightTransition(int id)
{
  if (id == activeTrans_)
    return;

  activeTrans_ = id;

  updateExtraSelections();
}

/*!
 * D&D files are forwarded to the main form.
 * Reimplements QPlainTextEdit::dropEvent
 */
void SourceEditor::dropEvent(QDropEvent * event)
{
  if (event->mimeData()->hasUrls()) {
    Q_ASSERT(!event->mimeData()->urls().isEmpty());
    QUrl url = event->mimeData()->urls().first();

    if (url.scheme() == "file") {
      QString path = event->mimeData()->urls().first().path();
// check for invalid paths (/C:/blabla)
#ifdef Q_OS_WIN32
      QRegExp drive_re("^/[A-Z]:");

      if (path.contains(drive_re))
        path.remove(0, 1);

#endif
      emit fileDropped(path);
    }
  }
  else {
    QPlainTextEdit::dropEvent(event);
  }
}

//! Reimplements QPlainTextEdit::dragEnterEvent.
void SourceEditor::dragEnterEvent(QDragEnterEvent * event)
{
  if (event->mimeData()->hasUrls())
    event->acceptProposedAction();
  else
    QPlainTextEdit::dragEnterEvent(event);
}

/*!
 * Handles key press events and processes code completion.
 * Reimplements QPlainTextEdit::keyPressEvent.
 */
void SourceEditor::keyPressEvent(QKeyEvent * event)
{
  QString eow(s_eow); // end of word

  // don't process the event if completion is disabled or we don't
  // have a completer ()
  if (!completer_) {
    handleKeyPressEvent(event);
    return;
  }

  if (completer_->popup()->isVisible()) {
    // The following keys are forwarded by the completer to the widget
    switch (event->key()) {
      case Qt::Key_Enter:
      case Qt::Key_Return:
      case Qt::Key_Escape:
      case Qt::Key_Tab:
      case Qt::Key_Backtab:
        event->ignore();
        return; // let the completer do default behavior
      default:
        // close the popup if we hit an invalid character
        if (!event->text().isEmpty() && eow.contains(event->text()))
          completer_->popup()->hide();
        break;
    }
  }

  // Ctrl+Space combo
  const bool isShortcut = event->modifiers() == Qt::ControlModifier &&
                          event->key() == Qt::Key_Space;

  if (!isShortcut)
    handleKeyPressEvent(event);

  if (isShortcut || completer_->popup()->isVisible()) {
    completer_->setCompletionPrefix(getCompletionPrefix());
    completer_->popup()->setCurrentIndex(
      completer_->completionModel()->index(0, 0));

    QAbstractItemModel * mdl = completer_->completionModel();

    if (mdl->rowCount() == 1) {
      insertCompletion(mdl->itemData(
                         mdl->index(0, 0))[Qt::DisplayRole].toString());
      completer_->popup()->hide();
    }
    else {
      QRect cr = cursorRect();
      cr.setWidth(completer_->popup()->sizeHintForColumn(0)
                  + completer_->popup()->verticalScrollBar()->sizeHint().width());

      // update popup!
      completer_->complete(cr);
    }
  }
}

//! Reimplements QPlainTextEdit::focusInEvent
void SourceEditor::focusInEvent(QFocusEvent * event)
{
  if (completer_)
    completer_->setWidget(this);

  QPlainTextEdit::focusInEvent(event);
}

/*!
 * Clears the highlighted transition when mouse leaves the window.
 * Reimplements QPlainTextEdit::leaveEvent.
 */
void SourceEditor::leaveEvent(QEvent * event)
{
  // reset the highlighted transition
  highlightTransition(-1);
  
  QPlainTextEdit::leaveEvent(event);
}

//! Reimplements QPlainTextEdit::mouseDoubleClickEvent.
void SourceEditor::mouseDoubleClickEvent(QMouseEvent * event)
{
  int trans = getTransitionFromPos(event->pos());

  if (!sim_ || trans == -1)
    QPlainTextEdit::mouseDoubleClickEvent(event);
  else
    sim_->step(trans);
}

//! Reimplements QPlainTextEdit::mouseMoveEvent.
void SourceEditor::mouseMoveEvent(QMouseEvent * event)
{
  QPlainTextEdit::mouseMoveEvent(event);

  if (!sim_)
    return;

  int trans = getTransitionFromPos(event->pos());

  highlightTransition(trans);
}

//! Reimplements QPlainTextEdit::resizeEvent.
void SourceEditor::resizeEvent(QResizeEvent * event)
{
  QPlainTextEdit::resizeEvent(event);

  QStyleOptionFrameV3 option;
  option.initFrom(this);

  QRect client = style()->subElementRect(
                   QStyle::SE_FrameContents, &option, this);

  QRect view = viewport()->rect();
  QRect visual = QStyle::visualRect(layoutDirection(), view, QRect(
                                      client.left(), client.top(), numbar_->width(), view.height() - view.top()));

  numbar_->setGeometry(visual);
}

//! Reimplements QPlainTextEdit::changeEvent.
void SourceEditor::changeEvent(QEvent * event)
{
  QPlainTextEdit::changeEvent(event);

  if (event->type() == QEvent::ApplicationFontChange ||
      event->type() == QEvent::FontChange) {

    QFont fnt = numbar_->font();
    fnt.setPointSize(font().pointSize());
    numbar_->setFont(fnt);

    updateSideBarWidth(blockCount());
    numbar_->update();
  }
}

void SourceEditor::handleKeyPressEvent(QKeyEvent * event)
{
  // handle line breaks
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    // auto-indent
    if (autoIndent_) {
      QString text = textCursor().block().text();
      uint fill = 0;

      while (text[fill].isSpace() && static_cast<int>(fill) < text.size())
        ++fill;

      // copy the padding to the next line
      textCursor().insertText("\n" + text.left(fill));

      return;
    }

    // convert tabs to spaces
  }
  else if (event->key() == Qt::Key_Tab && noTabs_) {
    const uint col = textCursor().columnNumber();
    const uint fill = (col / tabWidth_ + 1) * tabWidth_ - col;

    textCursor().insertText(QString(fill, ' '));
    return;
  }

  QPlainTextEdit::keyPressEvent(event);
}

void SourceEditor::sideBarPaintEvent(QPaintEvent * ev)
{
  if (!showLineNumbers_)
    return;

  QPainter painter(numbar_);

  const int barWidth = numbar_->width();
  const int lineSpacing = painter.fontMetrics().lineSpacing();
  const int fontHeight = painter.fontMetrics().height();

  QTextBlock block = firstVisibleBlock();

  int blockNumber = block.blockNumber();
  int top = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
  int bottom = top;

  while (block.isValid() && top <= ev->rect().bottom()) {
    top = bottom;
    bottom = top + blockBoundingRect(block).height();

    painter.drawText(0, top, barWidth - 4, fontHeight,
                     Qt::AlignRight, QString::number(blockNumber + 1));

    block = block.next();
    blockNumber = blockNumber + 1;
  }
}

int SourceEditor::getTransitionFromPos(const QPoint & pos)
{
  QList<int> extras = getExtraSelectionsFromPos(pos);

  int trans = INT_MAX;

  foreach(int sel, extras) {
    if (sel >= 0 && sel < transMap_.size())
      trans = qMin(trans, transMap_[sel]);
  }

  return trans != INT_MAX ? trans : -1;
}

const QList<int> SourceEditor::getExtraSelectionsFromPos(const QPoint & pos)
{
  QList<int> res;
  QTextCursor cur = cursorForPosition(pos);

  for (int i = 0; i < extraSelections().size(); ++i) {
    const QTextEdit::ExtraSelection itr = extraSelections().at(i);

    if (cur.position() >= itr.cursor.selectionStart() &&
        cur.position() < itr.cursor.selectionEnd()) {
      res.append(i);
    }
  }

  return res;
}

void SourceEditor::selectWordUnderCursor(QTextCursor & cur)
{
  QString eow(s_eow);

  int left, right;

  // left
  left = getCompletionPrefix().length();

  // right
  QTextCursor tc = cur;
  tc.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
  QString str = tc.selectedText();

  for (right = 0; right < str.length(); ++right) {
    if (eow.contains(str[right]))
      break;
  }

  cur.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, left);
  cur.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, left + right);
}

const QString SourceEditor::getCompletionPrefix(void)
{
  // get current prefix
  QTextCursor tc = textCursor();

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

QTextCursor SourceEditor::rectToCursor(const QRect & rect)
{
  QTextCursor res = textCursor();

  res.setPosition(0);
  res.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, rect.top());
  res.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, rect.left());
  res.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, rect.height() - 1);
  res.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
  res.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, rect.right() + 1);
  return res;
}

void SourceEditor::updateExtraSelections(void)
{
  QList<QTextEdit::ExtraSelection> list;

  transMap_.clear();

  // retrieve transitions from simulator

  if (sim_) {
    QString path = QUrl(document()->metaInformation(QTextDocument::DocumentUrl)).path();

    QTextEdit::ExtraSelection item;
    item.format.setBackground(normal_);

    Simulator::TransitionList trans;

    const Simulator * simulator = sim_->simulator();

    for (uint i = 0; i < simulator->transitionCount(); ++i) {
      // skip, since these are overridden later
      if (i == activeTrans_ || i == simulator->usedTransition())
        continue;

      trans = simulator->transitionSource(i);

      foreach(Simulator::TransitionPair itr, trans) {
        if (itr.first != path)
          continue;

        item.cursor = rectToCursor(itr.second);

        list.append(item);

        transMap_.append(i);
      }
    }

    // last used transitionCount
    if (simulator->usedTransition() != -1 && simulator->usedTransition() != activeTrans_) {
      trans = simulator->transitionSource(simulator->usedTransition());
      item.format.setBackground(lastUsed_);

      foreach(Simulator::TransitionPair itr, trans) {
        if (itr.first != path)
          continue;

        item.cursor = rectToCursor(itr.second);

        list.append(item);

        transMap_.append(simulator->usedTransition());
      }
    }

    // highlighted transition
    if (activeTrans_ != -1) {
      trans = simulator->transitionSource(activeTrans_);
      item.format.setBackground(hovered_);

      foreach(Simulator::TransitionPair itr, trans) {
        if (itr.first != path)
          continue;

        item.cursor = rectToCursor(itr.second);

        list.append(item);

        transMap_.append(activeTrans_);
      }
    }
  }

  // user highlights
  list.append(blocks_);

  setExtraSelections(list);
}

void SourceEditor::updateSideBarWidth(int blocks)
{
  const int wdth = showLineNumbers_ ? numbar_->calculateWidth(blocks) : 0;

  if (wdth != numbar_->width()) {
    // update margins
    if (isLeftToRight())
      setViewportMargins(wdth, 0, 0, 0);
    else
      setViewportMargins(0, 0, wdth, 0);

    // update width
    QRect rct = numbar_->geometry();

    // need to keep the offset!
    numbar_->setGeometry(rct.left(), rct.top(), wdth, numbar_->height());
  }
}

void SourceEditor::insertCompletion(const QString & completion)
{
  Q_ASSERT(completer_);

  if (completer_->widget() != this)
    return;

  QTextCursor tc = textCursor();

  // replaces current word with completion
  selectWordUnderCursor(tc);

  tc.insertText(completion);

  // doesn't erase current text
//   int extra = completion.length() - completer_->completionPrefix().length();
//   tc.insertText(completion.right(extra));
  setTextCursor(tc);
}

void SourceEditor::onSimulatorUpdate(void)
{
  activeTrans_ = -1;

  updateExtraSelections();
}

void SourceEditor::onUpdateRequest(const QRect & r, int dy)
{
  if (dy) {
    numbar_->scroll(0, dy);
    // wider than cursor width, not just cursor blinking
  }
  else if (r.width() > 4) {
    numbar_->update(0, r.y(), numbar_->width(), r.height());
  }

  if (r.contains(viewport()->rect()))
    updateSideBarWidth(blockCount());
}

void SourceEditor::onDocumentModified(bool state)
{
  emit documentModified(this, state);
}

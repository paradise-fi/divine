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

#ifndef ABSTRACT_EDITOR_H_
#define ABSTRACT_EDITOR_H_

#include <QWidget>

class QPrinter;

namespace divine {
namespace gui {

class AbstractDocument;

class AbstractEditor : public QWidget
{
    Q_OBJECT

  public:
    AbstractEditor(QWidget * parent = NULL) : QWidget(parent) {}
    virtual ~AbstractEditor() {}

    virtual bool isModified() const = 0;
    virtual QString viewName() const = 0;
    virtual QString sourcePath() const = 0;

    virtual AbstractDocument * document() = 0;

    virtual void print(QPrinter * printer) const = 0;

    virtual bool saveFile(const QString & path) = 0;
    virtual bool reload() = 0;

  signals:
    void modificationChanged(AbstractEditor * editor, bool changed);
};

}
}

#endif

// void MainForm::checkSyntax()
// {
//   Q_ASSERT(pageArea_->currentWidget());
//
//   AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
//   QUrl url(editor->document()->metaInformation(QTextDocument::DocumentUrl));
//
//   // if current page is untitled -> force save as
//   if(url.path().isEmpty()) {
//     if(!save(editor))
//       return;
//   }
//
//   // auto save
//   for(int i = 0; i < pageArea_->count(); ++i) {
//     editor = qobject_cast<AbstractEditor*>(pageArea_->widget(i));
//     url = editor->document()->metaInformation(QTextDocument::DocumentUrl);
//
//     if(editor->document()->isModified() && !url.path().isEmpty())
//       save(editor);
//   }
//
//   editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
//   url = editor->document()->metaInformation(QTextDocument::DocumentUrl);
//
//   SimulatorFactory * factory = simLoaders_[url.scheme()];
//   Q_ASSERT(factory);
//
//   Simulator * simulator = factory->load(url.path(), this);
//
//   if (!simulator)
//     return;
//
//   // clear highlights
//   for (int i = 0; i < pageArea_->count(); ++i) {
//     editor = qobject_cast<AbstractEditor*>(pageArea_->widget(i));
//     editor->resetHighlighting();
//   }
//
//   // check for errors
//   if(checkSyntaxErrors(simulator)) {
//     QMessageBox::warning(NULL, tr("Syntax Check"),
//                          tr("Source contains errors!"));
//   } else {
//     QMessageBox::information(NULL, tr("Syntax Check"),
//                              tr("No errors found."));
//   }
//
//   delete simulator;
// }
//
// void MainForm::findNext()
// {
//   AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
//
//   if (!editor)
//     return;
//
//   const QTextDocument::FindFlags flags = search_->flags();
//
//   QTextCursor cur = editor->document()->find(
//                       search_->pattern(), editor->textCursor(), flags);
//
//   if (cur.isNull()) {
//     cur = editor->document()->find(search_->pattern(), 0, flags);
//
//     if (cur.isNull())
//       statusBar()->showMessage(tr("No match"));
//     else
//       statusBar()->showMessage(tr("Passed the end of file"));
//   }
//
//   if (!cur.isNull())
//     editor->setTextCursor(cur);
// }
//
// void MainForm::findPrevious()
// {
//   AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
//
//   if (!editor)
//     return;
//
//   const QTextDocument::FindFlags flags = search_->flags() ^ QTextDocument::FindBackward;
//
//   QTextCursor cur = editor->document()->find(
//                       search_->pattern(), editor->textCursor(), flags);
//
//   if (cur.isNull()) {
//     cur = editor->textCursor();
//     cur.movePosition(QTextCursor::End);
//
//     cur = editor->document()->find(search_->pattern(), cur, flags);
//
//     if (cur.isNull())
//       statusBar()->showMessage(tr("No match"));
//     else
//       statusBar()->showMessage(tr("Passed the end of file"));
//   }
//
//   if (!cur.isNull())
//     editor->setTextCursor(cur);
// }
//
// void MainForm::replace()
// {
//   AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
//
//   if (!editor)
//     return;
//
//   if (editor->textCursor().hasSelection())
//     editor->textCursor().insertText(search_->sample());
//
//   findNext();
// }
//
// void MainForm::replaceAll()
// {
//   AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
//
//   if (!editor)
//     return;
//
//   const QTextDocument::FindFlags flags = search_->flags() & ~QTextDocument::FindBackward;
//
//   QTextCursor cur = editor->textCursor();
//
//   cur.movePosition(QTextCursor::Start);
//
//   do {
//     cur = editor->document()->find(search_->pattern(), cur, flags);
//     cur.insertText(search_->sample());
//   } while (!cur.isNull());
// }

// void MainForm::onOverwriteToggled(bool checked)
// {
//   AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
//
//   if (!editor)
//     return;
//
//   editor->setOverwriteMode(checked);
// }
//
// void MainForm::onWordWrapToggled(bool checked)
// {
//   AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
//
//   if (!editor)
//     return;
//
//   editor->setLineWrapMode(checked ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
// }
//
// void MainForm::onLineNumbersToggled(bool checked)
// {
//   AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
//
//   if (!editor)
//     return;
//
//   editor->setShowLineNumbers(checked);
// }

/*on tab current changed
//   // disconnect the previous editor
//   undoAct_->disconnect();
//   redoAct_->disconnect();
//   cutAct_->disconnect();
//   copyAct_->disconnect();
//   pasteAct_->disconnect();

  AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());

//   if (editor) {
//     // connect the new editor
//     connect(undoAct_, SIGNAL(triggered(bool)), editor, SLOT(undo()));
//     connect(redoAct_, SIGNAL(triggered(bool)), editor, SLOT(redo()));
//     connect(cutAct_, SIGNAL(triggered(bool)), editor, SLOT(cut()));
//     connect(copyAct_, SIGNAL(triggered(bool)), editor, SLOT(copy()));
//     connect(pasteAct_, SIGNAL(triggered(bool)), editor, SLOT(paste()));
//
//     overwriteAct_->setChecked(editor->overwriteMode());
//     wordWrapAct_->setChecked(editor->lineWrapMode() != QPlainTextEdit::NoWrap);
//     numbersAct_->setChecked(editor->showLineNumbers());
//   }
// */

/*
//! Displays the search dialog.
void MainForm::showFindDialog()
{
  QString hint;

  AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
  Q_ASSERT(editor);

  if (editor->textCursor().hasSelection()) {
    hint = editor->textCursor().selectedText();
  } else {
    QTextCursor cur = editor->textCursor();
    cur.select(QTextCursor::WordUnderCursor);
    hint = cur.selectedText();
  }

  search_->initialize(hint, false);

  search_->show();
  search_->activateWindow();
  search_->raise();
}*/

// //! Displays the search and replace dialog.
// void MainForm::showReplaceDialog()
// {
//   QString hint;
//
//   AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
//   Q_ASSERT(editor);
//
//   if (editor->textCursor().hasSelection()) {
//     hint = editor->textCursor().selectedText();
//   } else {
//     QTextCursor cur = editor->textCursor();
//   }
//
//   search_->initialize(hint, true);
//
//   search_->show();
//   search_->activateWindow();
//   search_->raise();
// }

// void MainForm::onFileChanged(const QString & fileName)
// {
//   AbstractEditor * edit = editor(fileName);
//
//   if (!edit)
//     return;
//
//   QFile file(fileName);
//
//   if (!file.open(QFile::ReadOnly | QFile::Text))
//     return;
//
//   QTextStream in(&file);
//
//   if (edit->toPlainText() != in.readAll())
//     edit->document()->setModified();
// }

// TODO: not all editors are text-based. ask ed for status? getting cursor position might nice for graphical editors
//   AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
//
//   if (!editor) {
//     statusLabel_->setText("");
//   } else {
//     QTextCursor cur = editor->textCursor();
//
//     statusLabel_->setText(tr("Line: %1 Col: %2").arg(QString::number(
//                             cur.blockNumber() + 1), QString::number(cur.columnNumber() + 1)));
//   }
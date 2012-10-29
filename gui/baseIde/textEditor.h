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

#ifndef TEXT_EDITOR_H_
#define TEXT_EDITOR_H_

#include <QString>
#include <QPlainTextEdit>
#include <QTextCharFormat>

#include "baseIdeExport.h"

#include "abstractEditor.h"
#include "abstractDocument.h"

#include "lineNumberBar.h"

class QPlainTextEdit;
class QTextCharFormat;

namespace divine {
namespace gui {

class MainForm;
class SearchPanel;

class BASE_IDE_EXPORT AbstractKeyPressHandler {
  public:
    virtual ~AbstractKeyPressHandler() {}

    virtual bool handleKeyPressEvent(
      QKeyEvent * event, const QRect * cursorRect) = 0;
};

class BASE_IDE_EXPORT EnhancedPlainTextEdit : public QPlainTextEdit,
                                              public LineNumberBarHost
{
  Q_OBJECT

  public:
    struct ExtraSelection {
      QPoint from;
      QPoint to;
      QTextCharFormat format;
      int id;   // < 0 is invalid
    };
    typedef QList<ExtraSelection> ExtraSelectionList;

  public:
    EnhancedPlainTextEdit(QWidget* parent = 0);

    int lineCount();
    int firstVisibleLine(const QRect & rect);
    QList<int> visibleLines(const QRect & rect);

    void addKeyEventHandler(AbstractKeyPressHandler * handler);

    // the last one is the topmost
    void setExtraSelections(const ExtraSelectionList & sels, bool reset);
    void requestExtraMouseEvents(bool state);

  public slots:
    void updateMargins(int width);

  signals:
    void fileDropped(const QString & path);
    void resized();

    void selectionDoubleClicked(int id);
    void selectionMouseOver(int id);

  protected:
    QList<AbstractKeyPressHandler*> handlers_;
    QList<int> selectionIds_;

    int selUnderCursor_;
    int leftMargin_;
    bool extraEvents_;

  protected:
    void dragEnterEvent(QDragEnterEvent * event);
    void dropEvent(QDropEvent * event);
    void hideEvent(QHideEvent * event);
    void resizeEvent(QResizeEvent * event);
    void keyPressEvent(QKeyEvent * event);
    void mouseDoubleClickEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent * event);

  private:
    QTextBlock firstVisibleBlock(const QRect & rect);
    int getSelectionFromPos(const QPoint & pos);
    QTextCursor rectToCursor(const QPoint & from, const QPoint & to);
    void checkSelOnPos(const QPoint & pos, bool reset);

};

class BASE_IDE_EXPORT TextEditor : public AbstractEditor
{
  Q_OBJECT

  public:
    TextEditor(MainForm * root, AbstractDocument * doc);
    ~TextEditor();

    bool isModified() const;
    QString viewName() const;
    QString sourcePath() const;

    AbstractDocument * document() {return doc_;}
    QTextDocument * textDocument();

    QTextCursor textCursor() const;
    void setTextCursor(const QTextCursor & cursor);

    void print(QPrinter * printer) const;

    bool openFile(const QString & path);
    bool saveFile(const QString & path);
    bool reload();

    // for auto-indenters, code-completers, etc.
    void addKeyEventHandler(AbstractKeyPressHandler * handler);

    // highlighting api
    void setExtraSelections(const EnhancedPlainTextEdit::ExtraSelectionList & sels, bool reset);
    void requestExtraMouseEvents(bool state);

  public slots:
    virtual void readSettings();
    virtual void updateActions();
    virtual void updateCursorInfo();

  protected:
    QString path_;

    EnhancedPlainTextEdit * edit_;
    LineNumberBar * lineBar_;
    SearchPanel * search_;

    AbstractDocument * doc_;
    MainForm * root_;

    bool active_;

  signals:
    void selectionDoubleClicked(int id);
    void selectionMouseOver(int id);

  protected:
    void resizeEvent(QResizeEvent * event);

  protected slots:
    virtual void onEditorActivated(AbstractEditor * editor);
    virtual void onEditorDeactivated();
    virtual void onEditorClosed(AbstractEditor * editor);

  private slots:
    void onModificationChanged(bool state);
    void onOverwriteToggled(bool state);
    void onLineWrapToggled(bool state);
    void onUpdateRequest(const QRect & r, int dy);
    void onFindTriggered();
    void onReplaceTriggered();

    void findNext();
    void findPrevious();
    void replace();
    void replaceAll();

  private:
    void showSearchPanel(bool replace);
    void search(bool forward);
};

}
}

#endif

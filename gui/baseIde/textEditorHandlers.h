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

#ifndef TEXT_EDITOR_HANDLERS_H_
#define TEXT_EDITOR_HANDLERS_H_

#include <QObject>
#include <QCompleter>

#include "baseIdeExport.h"

#include "textEditor.h"

namespace divine {
namespace gui {

class TextEditor;

class BASE_IDE_EXPORT SimpleIndenter : public QObject, public AbstractKeyPressHandler
{
  Q_OBJECT

  public:
    SimpleIndenter(TextEditor * editor);
    
    bool handleKeyPressEvent(QKeyEvent * event, const QRect * cursorRect);
      
  public slots:
    void readSettings();
    
  private:
    TextEditor * editor_;
    bool enabled_;
};

class BASE_IDE_EXPORT SimpleTabConverter : public QObject, public AbstractKeyPressHandler
{
  Q_OBJECT

  public:
    SimpleTabConverter(TextEditor * editor);
    
    bool handleKeyPressEvent(QKeyEvent * event, const QRect * cursorRect);
      
  public slots:
    void readSettings();
    
  private:
    TextEditor * editor_;
    uint tabWidth_;
    bool enabled_;
};

class BASE_IDE_EXPORT SimpleCompleter : public QCompleter, public AbstractKeyPressHandler
{
  Q_OBJECT

  public:
    SimpleCompleter(const QStringList & list, TextEditor * editor);
    
    bool handleKeyPressEvent(QKeyEvent * event, const QRect * cursorRect);
      
  private:
    TextEditor * editor_;
    
  private:
    void selectWordUnderCursor(QTextCursor & cur) const;
    QString getCompletionPrefix() const;

  private slots:
    void insertCompletion(const QString & text);
};

}
}

#endif

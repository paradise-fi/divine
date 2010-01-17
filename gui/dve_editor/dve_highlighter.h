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

#ifndef DVE_HIGHLIGHTER_H_
#define DVE_HIGHLIGHTER_H_

#include <QSyntaxHighlighter>

class QTextDocument;

/*!
 * This class provides syntax highlighting for DVE files.
 */
class DveHighlighter : public QSyntaxHighlighter {
  Q_OBJECT
  public:
    DveHighlighter(bool macros, QTextDocument * parent);
    ~DveHighlighter() {}

  public slots:
    void readSettings(void);

  protected:
    void highlightBlock(const QString & text);

  private:
    enum HighlightStyle {
      hsDefault = 0,
      hsNumber,
      hsDataType,
      hsTransition,
      hsKeyword,
      hsComment,
      hsMacro,
      hsLast
    };

    typedef QPair<QRegExp, HighlightStyle> PatternPair;
    typedef QList<PatternPair> PatternList;
    
  private:
    QTextCharFormat styles_[hsLast];
    PatternList patterns_;

  private:
    void highlightRange(const QString & text, int offset);
};

#endif

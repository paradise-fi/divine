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

#include <algorithm>

#include <QSettings>

#include "dve_highlighter.h"
#include "settings.h"
#include "prefs_dve.h"

DveHighlighter::DveHighlighter(bool macros, QTextDocument * parent)
    : QSyntaxHighlighter(parent)
{
  // fill in the pattern list
  if(macros) {
    patterns_.append(qMakePair(QRegExp("\\b(forloop|false|true|bool|stack|queue|"
                                      "full|empty|push|pop|top|front|pop_front|"
                                      "buffer_channel|async_channel|loosy_channel|"
                                      "bounded_loosy_channel|system|default|define)\\b"), hsMacro));
  }
  
  patterns_.append(qMakePair(QRegExp("\\b(process|state|init|trans|channel|"
                                     "sync|system|assert|guard|effect|commit|"
                                     "const|async|accept|property)\\b"), hsKeyword));
  patterns_.append(qMakePair(QRegExp("\\b(byte|int)\\b"), hsDataType));
  patterns_.append(qMakePair(QRegExp("\\b(true|false)\\b"), hsKeyword));
  patterns_.append(qMakePair(QRegExp("\\b(imply|and|or|not)\\b"), hsKeyword));
  patterns_.append(qMakePair(QRegExp("->"), hsTransition));
  patterns_.append(qMakePair(QRegExp("\\b[0-9]+\\b"), hsNumber));

  // restore
  readSettings();
}

//! Reloads settings.
void DveHighlighter::readSettings(void)
{
  QSettings & s = sSettings();

  QString fore, back;
  bool bold, italic, under;

  s.beginReadArray("DVE Syntax");

  for (uint i = 0; i < hsLast; ++i) {
    s.setArrayIndex(i);

    bold = s.value("bold", dveDefs[i].bold).toBool();
    italic = s.value("italic", dveDefs[i].italic).toBool();
    under = s.value("underline", dveDefs[i].underline).toBool();
    fore = s.value("foreground", dveDefs[i].foreground).toString();
    back = s.value("background", dveDefs[i].background).toString();

    styles_[i] = QTextCharFormat();
    styles_[i].setFontWeight(bold ? QFont::Bold : QFont::Normal);
    styles_[i].setFontItalic(italic);
    styles_[i].setFontUnderline(under);

    if (!fore.isEmpty())
      styles_[i].setForeground(QColor(fore));

    if (!back.isEmpty())
      styles_[i].setBackground(QColor(back));
  }

  s.endArray();

  rehighlight();
}

void DveHighlighter::highlightBlock(const QString & text)
{
  setCurrentBlockState(previousBlockState());

  if (text.isEmpty())
    return;

  int from_x = 0, to_x = text.length();
  int index;
  
  QRegExp comment_re("(//|/\\*)");
  
  // clear previous comments
  if(previousBlockState() == 1) {
    index = text.indexOf("*/");

    if(index != -1) {
      from_x = index + 2;
      setCurrentBlockState(-1);
    } else {
      from_x = to_x;
      setCurrentBlockState(1);
    }
    setFormat(0, from_x, styles_[hsComment]);
  }
  
  // comments
  while((index = text.indexOf(comment_re, from_x)) != -1) {
    highlightRange(text.mid(from_x, index - from_x), from_x);
    from_x = index;
    
    // single-line comment
    if(comment_re.capturedTexts().at(1) == "//") {  
      setFormat(from_x, to_x - from_x, styles_[hsComment]);
      from_x = to_x;
    // multi-line comment
    } else if((index = text.indexOf("*/", from_x + 2)) == -1) {
      setFormat(from_x, to_x - from_x, styles_[hsComment]);
      from_x = to_x;
      setCurrentBlockState(1);
    } else {
      setFormat(from_x, index - from_x + 2, styles_[hsComment]);
      from_x = index + 2;
    }
  }

  // remainder of the line
  highlightRange(text.right(to_x - from_x), from_x);
}

void DveHighlighter::highlightRange(const QString & text, int offset)
{
  if (text.isEmpty())
    return;

  // default
  setFormat(offset, text.length(), styles_[hsDefault]);
  
  foreach(PatternList::value_type itr, patterns_) {
    int index = text.indexOf(itr.first);
    while (index >= 0) {
      int length = itr.first.matchedLength();

      QTextCharFormat fmt = styles_[itr.second];

      setFormat(offset + index, length, fmt);
      index = text.indexOf(itr.first, index + length);
    }
  }
}

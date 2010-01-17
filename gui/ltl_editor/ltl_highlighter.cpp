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

#include "ltl_highlighter.h"
#include "settings.h"
#include "prefs_ltl.h"

LtlHighlighter::LtlHighlighter(QTextDocument * parent)
    : QSyntaxHighlighter(parent)
{
  // fill in the pattern list
  patterns_.append(qMakePair(QRegExp("^\\s*#define\\s+\\S.*$"), hsDefinition));
  patterns_.append(qMakePair(QRegExp("^\\s*#property\\s+\\S.*$"), hsProperty));

  // restore
  readSettings();
}

//! Reloads settings.
void LtlHighlighter::readSettings(void)
{
  QSettings & s = sSettings();

  QString fore, back;
  bool bold, italic, under;

  s.beginReadArray("LTL Syntax");

  for (uint i = 0; i < hsLast; ++i) {
    s.setArrayIndex(i);

    bold = s.value("bold", ltlDefs[i].bold).toBool();
    italic = s.value("italic", ltlDefs[i].italic).toBool();
    under = s.value("underline", ltlDefs[i].underline).toBool();
    fore = s.value("foreground", ltlDefs[i].foreground).toString();
    back = s.value("background", ltlDefs[i].background).toString();

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

void LtlHighlighter::highlightBlock(const QString & text)
{
  if (text.isEmpty())
    return;

  // default
  setFormat(0, text.length(), styles_[hsDefault]);
  
  foreach(PatternList::value_type itr, patterns_) {
    int index = text.indexOf(itr.first);
    while (index >= 0) {
      int length = itr.first.matchedLength();

      QTextCharFormat fmt = styles_[itr.second];

      setFormat(index, length, fmt);
      index = text.indexOf(itr.first, index + length);
    }
  }
}

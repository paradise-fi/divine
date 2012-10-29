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

#include "settings.h"

#include "ui_editorPreferences.h"

#include "editorPreferences.h"
#include "plugins.h"

namespace divine {
namespace gui {

// defaults
EditorPreferences::EditorPreferences(QWidget * parent)
    : QWidget(parent)
{
  ui_ = new Ui::EditorPreferences();
  ui_->setupUi(this);
}

EditorPreferences::~EditorPreferences()
{
  delete ui_;
}

void EditorPreferences::readSettings()
{
  Settings s("ide/editor");

  QFont fnt = s.value("font", defEditorFont()).value<QFont>();

  ui_->fontSelect->setCurrentFont(fnt);
  ui_->sizeSpin->setValue(fnt.pointSize());

  ui_->indentSpin->setValue(s.value("tabWidth", defEditorTabWidth).toUInt());
  ui_->noTabsBox->setChecked(s.value("convertTabs", defEditorConvertTabs).toBool());
  ui_->indentBox->setChecked(s.value(
    "autoIndent", defEditorAutoIndent).toBool());
}

void EditorPreferences::writeSettings()
{
  Settings s("ide/editor");

  QFont fnt(ui_->fontSelect->currentFont());
  fnt.setPointSize(ui_->sizeSpin->value());

  s.setValue("font", fnt);
  s.setValue("tabWidth", ui_->indentSpin->value());
  s.setValue("convertTabs", ui_->noTabsBox->isChecked());
  s.setValue("autoIndent", ui_->indentBox->isChecked());
}

}
}

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

#include "ui_prefs_editor.h"

#include "prefs_editor.h"
#include "plugins.h"

// defaults
EditorPreferences::EditorPreferences(QWidget * parent)
    : PreferencesPage(parent)
{
  ui_ = new Ui::EditorPreferences();
  ui_->setupUi(this);
}

EditorPreferences::~EditorPreferences()
{
  delete ui_;
}

void EditorPreferences::readSettings(void)
{
  QSettings & s  = sSettings();

  s.beginGroup("Editor");

  QFont fnt = s.value("font", defEditorFont()).value<QFont>();

  ui_->fontSelect->setCurrentFont(fnt);
  ui_->sizeSpin->setValue(fnt.pointSize());

  ui_->indentSpin->setValue(s.value("tabWidth", defEditorTabWidth).toUInt());
  ui_->noTabsBox->setChecked(s.value("notabs", defEditorNoTabs).toBool());
  ui_->indentBox->setChecked(s.value(
    "autoIndent", defEditorAutoIndent).toBool());

  s.endGroup();
}

void EditorPreferences::writeSettings(void)
{
  QSettings & s  = sSettings();

  s.beginGroup("Editor");

  QFont fnt(ui_->fontSelect->currentFont());
  fnt.setPointSize(ui_->sizeSpin->value());

  s.setValue("font", fnt);
  s.setValue("tabWidth", ui_->indentSpin->value());
  s.setValue("noTabs", ui_->noTabsBox->isChecked());
  s.setValue("autoIndent", ui_->indentBox->isChecked());

  s.endGroup();
}

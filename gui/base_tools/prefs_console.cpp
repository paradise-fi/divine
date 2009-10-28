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

#include <QColorDialog>

#include "ui_prefs_console.h"

#include "prefs_console.h"
#include "settings.h"

// defaults
#include "base_tools.h"

ConsolePreferences::ConsolePreferences(QWidget * parent)
    : PreferencesPage(parent)
{
  ui_ = new Ui::ConsolePreferences();
  ui_->setupUi(this);
}

ConsolePreferences::~ConsolePreferences()
{
  delete ui_;
}

void ConsolePreferences::readSettings(void)
{
  QSettings & s  = sSettings();

  s.beginGroup("Console");

  QFont fnt = s.value("font", defConsoleFont()).value<QFont>();

  ui_->fontSelect->setCurrentFont(fnt);
  ui_->sizeSpin->setValue(fnt.pointSize());
  ui_->editorBox->setChecked(s.value(
                               "useEditorFont", defConsoleEFont).toBool());

  ui_->foreButton->setPalette(QPalette(
    s.value("foreground", defConsoleFore).value<QColor>()));
  ui_->backButton->setPalette(QPalette(
    s.value("background", defConsoleBack).value<QColor>()));
  ui_->syscolBox->setChecked(s.value(
    "useSystemColours", defConsoleSysColours).toBool());

  s.endGroup();
}

void ConsolePreferences::writeSettings(void)
{
  QSettings & s  = sSettings();

  s.beginGroup("Console");

  QFont fnt(ui_->fontSelect->currentFont());
  fnt.setPointSize(ui_->sizeSpin->value());

  s.setValue("font", fnt);
  s.setValue("useEditorFont", ui_->editorBox->isChecked());
  s.setValue("useSystemColours", ui_->syscolBox->isChecked());

  s.setValue("foreground", ui_->foreButton->palette().color(QPalette::Button));
  s.setValue("background", ui_->backButton->palette().color(QPalette::Button));

  s.endGroup();
}

void ConsolePreferences::on_editorBox_stateChanged(int state)
{
  const bool st = !(state == Qt::Checked);

  ui_->fontSelect->setEnabled(st);
  ui_->sizeSpin->setEnabled(st);
  ui_->fontLabel->setEnabled(st);
  ui_->sizeLabel->setEnabled(st);
}

void ConsolePreferences::on_syscolBox_stateChanged(int state)
{
  const bool st = !(state == Qt::Checked);

  ui_->foreButton->setEnabled(st);
  ui_->backButton->setEnabled(st);
  ui_->foreLabel->setEnabled(st);
  ui_->backLabel->setEnabled(st);
}

void ConsolePreferences::on_foreButton_clicked(void)
{
  const QColor color = QColorDialog::getColor(
  ui_->foreButton->palette().color(QPalette::Button), this);

  if (color.isValid())
    ui_->foreButton->setPalette(QPalette(color));
}

void ConsolePreferences::on_backButton_clicked(void)
{
  const QColor color = QColorDialog::getColor(
    ui_->backButton->palette().color(QPalette::Button), this);

  if (color.isValid())
    ui_->backButton->setPalette(QPalette(color));
}

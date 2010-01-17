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

#include "ui_prefs_output.h"

#include "prefs_output.h"
#include "settings.h"

// defaults
#include "base_tools.h"

OutputPreferences::OutputPreferences(QWidget * parent)
    : PreferencesPage(parent)
{
  ui_ = new Ui::OutputPreferences();
  ui_->setupUi(this);
}

OutputPreferences::~OutputPreferences()
{
  delete ui_;
}

void OutputPreferences::readSettings(void)
{
  QSettings & s  = sSettings();

  s.beginGroup("Output");

  QFont fnt = s.value("font", defOutputFont()).value<QFont>();

  ui_->fontSelect->setCurrentFont(fnt);
  ui_->sizeSpin->setValue(fnt.pointSize());
  ui_->editorBox->setChecked(s.value(
                               "useEditorFont", defOutputEFont).toBool());

  ui_->foreButton->setCurrentColor(
    s.value("foreground", defOutputFore).value<QColor>());
  ui_->backButton->setCurrentColor(
    s.value("background", defOutputBack).value<QColor>());
  ui_->syscolBox->setChecked(s.value(
    "useSystemColors", defOutputSysColors).toBool());

  s.endGroup();
}

void OutputPreferences::writeSettings(void)
{
  QSettings & s  = sSettings();

  s.beginGroup("Output");

  QFont fnt(ui_->fontSelect->currentFont());
  fnt.setPointSize(ui_->sizeSpin->value());

  s.setValue("font", fnt);
  s.setValue("useEditorFont", ui_->editorBox->isChecked());
  s.setValue("useSystemColors", ui_->syscolBox->isChecked());

  s.setValue("foreground", ui_->foreButton->currentColor());
  s.setValue("background", ui_->backButton->currentColor());

  s.endGroup();
}

void OutputPreferences::on_editorBox_stateChanged(int state)
{
  const bool st = !(state == Qt::Checked);

  ui_->fontSelect->setEnabled(st);
  ui_->sizeSpin->setEnabled(st);
  ui_->fontLabel->setEnabled(st);
  ui_->sizeLabel->setEnabled(st);
}

void OutputPreferences::on_syscolBox_stateChanged(int state)
{
  const bool st = !(state == Qt::Checked);

  ui_->foreButton->setEnabled(st);
  ui_->backButton->setEnabled(st);
  ui_->foreLabel->setEnabled(st);
  ui_->backLabel->setEnabled(st);
}

// void OutputPreferences::on_foreButton_clicked(void)
// {
//   const QColor color = QColorDialog::getColor(
//   ui_->foreButton->palette().color(QPalette::Button), this);
// 
//   if (color.isValid())
//     ui_->foreButton->setPalette(QPalette(color));
// }
// 
// void OutputPreferences::on_backButton_clicked(void)
// {
//   const QColor color = QColorDialog::getColor(
//     ui_->backButton->palette().color(QPalette::Button), this);
// 
//   if (color.isValid())
//     ui_->backButton->setPalette(QPalette(color));
// }

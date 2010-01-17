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

#include "ui_prefs_simulator.h"

#include "prefs_simulator.h"
#include "settings.h"

// defaults
#include "base_tools.h"

SimulatorPreferences::SimulatorPreferences(QWidget * parent)
  : PreferencesPage(parent)
{
  ui_ = new Ui::SimulatorPreferences();
  ui_->setupUi(this);
}

SimulatorPreferences::~SimulatorPreferences()
{
  delete ui_;
}

void SimulatorPreferences::readSettings(void)
{
  QSettings & s  = sSettings();

  s.beginGroup("Simulator");

  ui_->randomBox->setChecked(s.value("random", defSimulatorRandom).toBool());
  ui_->delaySpin->setValue(s.value("delay", defSimulatorDelay).toUInt());
  ui_->seedSpin->setValue(s.value("seed", defSimulatorSeed).toUInt());
  ui_->stepsSpin->setValue(s.value("steps", defSimulatorSteps).toUInt());

  ui_->normalButton->setCurrentColor(
    s.value("normal", defSimulatorNormal).value<QColor>());
  ui_->highlightButton->setCurrentColor(
    s.value("highlight", defSimulatorHighlight).value<QColor>());

  s.endGroup();
}

void SimulatorPreferences::writeSettings(void)
{
  QSettings & s  = sSettings();

  s.beginGroup("Simulator");

  s.setValue("random", ui_->randomBox->isChecked());
  s.setValue("delay", ui_->delaySpin->value());
  s.setValue("seed", ui_->seedSpin->value());
  s.setValue("steps", ui_->stepsSpin->value());

  s.setValue("normal", ui_->normalButton->currentColor());
  s.setValue("highlight", ui_->highlightButton->currentColor());

  s.endGroup();
}

void SimulatorPreferences::on_randomBox_stateChanged(int state)
{
  const bool st = !(state == Qt::Checked);

  ui_->seedSpin->setEnabled(st);
  ui_->seedLabel->setEnabled(st);
}

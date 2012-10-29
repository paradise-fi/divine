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

#include "ui_simulatorPreferences.h"

#include "simulatorPreferences.h"
#include "settings.h"

// defaults
#include "baseToolsPlugin.h"

namespace divine {
namespace gui {

SimulatorPreferences::SimulatorPreferences(QWidget * parent)
  : QWidget(parent)
{
  ui_ = new Ui::SimulatorPreferences();
  ui_->setupUi(this);
}

SimulatorPreferences::~SimulatorPreferences()
{
  delete ui_;
}

void SimulatorPreferences::readSettings()
{
  Settings sim("ide/simulator");

  ui_->randomBox->setChecked(sim.value("random", defSimulatorRandom).toBool());
  ui_->delaySpin->setValue(sim.value("delay", defSimulatorDelay).toUInt());
  ui_->seedSpin->setValue(sim.value("seed", defSimulatorSeed).toUInt());
  ui_->stepsSpin->setValue(sim.value("steps", defSimulatorSteps).toUInt());
  
  Settings ver("ide/verification");
  
  ui_->algorithmBox->setCurrentIndex(ui_->algorithmBox->findText(ver.value("algorithm", defVerificationAlgorithm).toString()));
  ui_->threadsSpin->setValue(ver.value("threads", defVerificationThreads).toInt());
}

void SimulatorPreferences::writeSettings()
{
  Settings sim("ide/simulator");

  sim.setValue("random", ui_->randomBox->isChecked());
  sim.setValue("delay", ui_->delaySpin->value());
  sim.setValue("seed", ui_->seedSpin->value());
  sim.setValue("steps", ui_->stepsSpin->value());

  Settings ver("ide/verification");
  
  ver.setValue("algorithm", ui_->algorithmBox->currentText());
  ver.setValue("threads", ui_->threadsSpin->value());
}

void SimulatorPreferences::on_randomBox_stateChanged(int state)
{
  const bool st = !(state == Qt::Checked);

  ui_->seedSpin->setEnabled(st);
  ui_->seedLabel->setEnabled(st);
}

}
}

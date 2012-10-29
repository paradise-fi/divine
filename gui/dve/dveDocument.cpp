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

#include "settings.h"
#include "mainForm.h"
#include "simulationProxy.h"
#include "textEditor.h"

#include "dveDocument.h"
#include "dveSimulator.h"
#include <qtextstream.h>

namespace divine {
namespace gui {

namespace {
  void setupTransition(SimulationProxy * sim, int id, const QColor & color,
                       EnhancedPlainTextEdit::ExtraSelectionList & out)
  {
    Q_ASSERT(qobject_cast<DveSimulator*>(sim->simulator()));
    const DveTransition * dtr = static_cast<const DveTransition*>(sim->transition(id));
    
    EnhancedPlainTextEdit::ExtraSelection esel;
    esel.format.setBackground(color);
    esel.id = id;

    foreach(QRect rect, dtr->source) {
      esel.from = rect.topLeft();
      esel.to = rect.bottomRight();
      out.append(esel);
    }
  }
}
  
DveDocument::DveDocument(MainForm * root)
  : TextDocument(root),
    root_(root),
    sim_(root->simulation()),
    edit_(qobject_cast<TextEditor*>(editor(mainView()))),
    normal_(defSimulatorNormal),
    lastUsed_(defSimulatorUsed),
    hovered_(defSimulatorHighlight)
{
  Q_ASSERT(edit_);
  
  edit_->requestExtraMouseEvents(true);
  
  connect(root_, SIGNAL(settingsChanged()), SLOT(readSettings()));
  
  connect(sim_, SIGNAL(currentIndexChanged(int)), SLOT(resetTransitions()));
  connect(sim_, SIGNAL(reset()), SLOT(resetTransitions()));
  connect(sim_, SIGNAL(stopped()), SLOT(resetTransitions()));
  connect(sim_, SIGNAL(stateChanged(int)), SLOT(resetTransitions()));
  
  connect(sim_, SIGNAL(highlightTransition(int)), SLOT(highlightTransition(int)));
  
  connect(edit_, SIGNAL(selectionDoubleClicked(int)), sim_, SLOT(step(int)));
  connect(edit_, SIGNAL(selectionMouseOver(int)), sim_, SIGNAL(highlightTransition(int)));
  
  readSettings();
}

void DveDocument::readSettings()
{
  // transition colours
  Settings s("ide/simulator/colours");

  normal_ = s.value("normal", defSimulatorNormal).value<QColor>();
  lastUsed_ = s.value("used", defSimulatorUsed).value<QColor>();
  hovered_ = s.value("highlight", defSimulatorHighlight).value<QColor>();
  
  resetTransitions();
}

void DveDocument::resetTransitions()
{
  updateTransitions(-1, true);
}

void DveDocument::highlightTransition(int focus)
{
  updateTransitions(focus, false);
}

void DveDocument::updateTransitions(int focus, bool reset)
{
  static int cnt = 0;
  if(!root_->isLocked(this))
    return;
  
  EnhancedPlainTextEdit::ExtraSelectionList selections;
  int used = sim_->usedTransition();

  for(int i=0; i < sim_->transitionCount(); ++i) {
    if(i == focus || i == used)
      continue;
    
    setupTransition(sim_, i, normal_, selections);
  }
  // ordered by visibility
  if(used >= 0 && used < sim_->transitionCount())
    setupTransition(sim_, used, lastUsed_, selections);

  if(focus >= 0 && focus < sim_->transitionCount())
    setupTransition(sim_, focus, hovered_, selections);

  edit_->setExtraSelections(selections, reset);
}

}
}

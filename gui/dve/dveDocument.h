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

#ifndef DVE_DOCUMENT_H_
#define DVE_DOCUMENT_H_

#include <QColor>

#include "textDocument.h"

namespace divine {
namespace gui {

class MainForm;
class SimulationProxy;
class TextEditor;

class MDveDocument : public TextDocument
{
  Q_OBJECT

  public:
    MDveDocument(MainForm * root) : TextDocument(root) {}
};

class DveDocument : public TextDocument
{
  Q_OBJECT

  public:
    DveDocument(MainForm * root);

  public slots:
    void readSettings();
    void resetTransitions();
    void highlightTransition(int focus);

  protected:
    MainForm * root_;
    SimulationProxy * sim_;
    TextEditor * edit_;

    // auto-highlight colours
    QColor normal_;
    QColor lastUsed_;
    QColor hovered_;

  protected:
    void updateTransitions(int focus, bool reset);
};

}
}

#endif



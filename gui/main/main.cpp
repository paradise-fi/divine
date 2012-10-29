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

#include <QApplication>

#include "baseIde/modules.h"
#include "baseTools/baseToolsModule.h"
#include "dve/dveModule.h"

#include "mainForm.h"

using namespace divine::gui;

void loadModules(MainForm * root)
{
  
}

int main(int argc, char * argv[])
{
  QApplication app(argc, argv);
  MainForm mf;

  // load modules
  BaseToolsModule btm;
  btm.install(&mf);
  
  DveModule dm;
  dm.install(&mf);

  // show time
  mf.initialize();
  mf.show();
  return app.exec();
}

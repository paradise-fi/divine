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

#ifndef LTL_DOCUMENT_H_
#define LTL_DOCUMENT_H_

#include "textDocument.h"

namespace divine {
namespace gui {

class MainForm;

class LtlDocument : public TextDocument
{
  Q_OBJECT

  public:
    LtlDocument(MainForm * root) : TextDocument(root) {}
};

}
}

#endif



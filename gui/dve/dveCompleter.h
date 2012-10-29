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

#ifndef DVE_DOCUMENT_HANDLERS_H_
#define DVE_DOCUMENT_HANDLERS_H_

class BASE_IDE_EXPORT AbstractKeyPressHandler {
  public:
    virtual ~AbstractKeyPressHandler() {}
    
    virtual bool handleKeyPressEvent(
      QKeyEvent * event,
      QTextDocument * doc,
      const QTextCursor * cursor,
      const QRect * cursorRect) const = 0;
};

namespace divine {
namespace gui {

  class 
  
}
}

#endif

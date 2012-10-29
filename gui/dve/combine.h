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

#ifndef COMBINE_H_
#define COMBINE_H_

#include <QByteArray>

class QStringList;

namespace divine {
namespace gui {

class Combine
{
  public:
    Combine() {}

    // does ONLY preprocessing of Mxxx files and nothing else
    QByteArray preprocess(const QByteArray & input, const QStringList & defs);

    // pure DVE + LTL combine
    QByteArray combine(const QByteArray & dve, const QByteArray & ltl, int property);

  private:
    QByteArray process_ltl(int property);
    QByteArray ltl_to_dve(std::string ltl);

  private:
    // merging code from tools/combine.h
    std::string in_data_;
    std::string ltl_data_;
    std::string system_;

    std::vector<std::string> ltl_formulae_;
    std::string ltl_defs_;
};

}
}

#endif

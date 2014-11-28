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

#include <sstream>

#include <divine/ltl2ba/main.h>

#include <brick-string.h>
#include <brick-process.h>

#include <QStringList>
#include <QMessageBox>

#include <tools/combine.m4.h>

#include "combine.h"

namespace divine {
namespace gui {

namespace {

  bool conditionNBA = true;

  std::string m4( std::string in, const std::string & defs )
  {
    brick::process::PipeThrough p( "m4" + defs );
    return p.run( in );
  }

  std::string cpp( std::string in )
  {
    brick::process::PipeThrough p( "cpp -E -P" );
    return p.run( in );
  }
}

QByteArray Combine::preprocess(const QByteArray & input, const QStringList & defs)
{
  std::stringstream ss;

  foreach(const QString & def, defs) {
    ss << " -D" << def.toStdString();
  }

  std::string in_data = input.data() + std::string("\n");
  return m4(combine_m4 + in_data, ss.str()).c_str();
}

QByteArray Combine::combine(const QByteArray & dve, const QByteArray & ltl, int property)
{
  in_data_ = dve.data() + std::string("\n");
  ltl_data_ = ltl.data() + std::string("\n");

  // Combine::combine_dve
  int off = in_data_.find( "system async" );
  if ( off != std::string::npos ) {
    system_ = "system async property LTL_property;\n";
  } else {
    off = in_data_.find( "system sync" );
    if ( off != std::string::npos ) {
      system_ = "system sync property LTL_property;\n";
    } else {
      QMessageBox::warning(
        NULL, QObject::tr("DiVinE IDE"), QObject::tr(
          "DVE has to specify either ""'system sync' or 'system async'."));
      return QByteArray();
    }
  }
  in_data_ = std::string( in_data_, 0, off );

  return process_ltl(property);
}

QByteArray Combine::process_ltl(int property)
{
  brick::string::Splitter lines( "\n", 0 );
  brick::string::ERegexp prop( "^[ \t]*#property ([^\n]+)", 2 );
  brick::string::ERegexp def( "^[ \t]*#define ([^\n]+)", 2 );

  std::vector< std::string >::iterator i;
  for ( brick::string::Splitter::const_iterator ln = lines.begin( ltl_data_ ); ln != lines.end(); ++ln ) {
    if ( prop.match( *ln ) )
      ltl_formulae_.push_back( prop[1] );
    if ( def.match( *ln ) )
      ltl_defs_ += "\n#define " + def[1];
  }

  Q_ASSERT(property >= 0 && property < ltl_formulae_.size());
  return ltl_to_dve(ltl_formulae_[property]);
}

QByteArray Combine::ltl_to_dve(std::string ltl)
{
  std::string automaton;
  std::stringstream automaton_str;

  divine::output( buchi( ltl, false ), automaton_str );
  automaton = automaton_str.str();

  std::string prop = cpp( ltl_defs_ + "\n" + automaton );
  std::string dve = cpp( in_data_ + "\n" + prop + "\n" + system_ );
  return dve.c_str();
}

}
}

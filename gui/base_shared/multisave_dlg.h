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

#ifndef MULTISAVE_DLG_H_
#define MULTISAVE_DLG_H_

#include <QDialog>

namespace Ui
{
  class MultiSaveDialog;
}

class SourceEditor;

/*!
 * The MultiSaveDialog class implements the dialog window asking the user
 * to save multiple files.
 */
class MultiSaveDialog : public QDialog
{
    Q_OBJECT

  public:
    MultiSaveDialog(const QList<SourceEditor*> & files, QWidget * parent = NULL);
    ~MultiSaveDialog();

    const QList<SourceEditor*> selection(void) const;

  private:
    Ui::MultiSaveDialog * ui_;

  private slots:
    void on_noneButton_clicked(void);
};

#endif


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

#ifndef MULTI_SAVE_DIALOG_H_
#define MULTI_SAVE_DIALOG_H_

#include <QDialog>

namespace Ui
{
  class MultiSaveDialog;
}

namespace divine {
namespace gui {

class AbstractEditor;

/*!
 * The MultiSaveDialog class implements the dialog window asking the user
 * to save multiple files.
 */
class MultiSaveDialog : public QDialog
{
    Q_OBJECT

  public:
    MultiSaveDialog(const QList<AbstractEditor*> & files, QWidget * parent = NULL);
    ~MultiSaveDialog();

    const QList<AbstractEditor*> selection() const;

  private:
    Ui::MultiSaveDialog * ui_;

  private slots:
    void on_noneButton_clicked();
};

}
}

#endif

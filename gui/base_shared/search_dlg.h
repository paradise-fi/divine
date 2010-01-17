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

#ifndef SEARCH_DLG_H_
#define SEARCH_DLG_H_

#include <QTextDocument>
#include <QDialog>

namespace Ui
{
  class SearchDialog;
}

/*!
 * The SearchDialog class implements the Find (and replace) dilaog window.
 */
class SearchDialog : public QDialog
{
    Q_OBJECT

  public:
    SearchDialog(QWidget * parent = NULL);
    ~SearchDialog();

    void initialize(const QString & hint, bool replace);

    const QRegExp pattern(void) const;
    const QString sample(void) const;
    QTextDocument::FindFlags flags(void) const;
    
  signals:
    void findNext(void);    //!< User has clicked the Find Next button.
    void replace(void);     //!< User has clicked the Replace button.
    void replaceAll(void);  //!< User has clicked the Replace All button.
    
  private:
    Ui::SearchDialog * ui_;

  private:
    void updateButtonState(void);
    
  private slots:
    void on_findEdit_textChanged(void);
    void on_replaceEdit_textChanged(void);
    void on_regularBox_stateChanged(int state);
};

#endif


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

#ifndef SEARCH_PANEL_H_
#define SEARCH_PANEL_H_

#include <QWidget>

namespace Ui
{
  class SearchPanel;
}

namespace divine {
namespace gui {
 
/*!
 * The SearchDialog class implements the Find (and replace) dilaog window.
 */
class SearchPanel : public QWidget
{
    Q_OBJECT

  public:
    SearchPanel(QWidget * parent = NULL);
    ~SearchPanel();

    const bool modeRegEx() const;
    const bool modeCaseSensitive() const;
    const bool modeWholeWords() const;
    
    const QRegExp pattern() const;
    const QString sample() const;
    
  public slots:
    void show(const QString & hint, bool replace);
    
  signals:
    void closed();
    
    void searchAvailable(bool state);
    
    void findNext();        //!< User has clicked the Next button.
    void findPrevious();    //!< User has clicked the Previous button.
    void replace();         //!< User has clicked the Replace button.
    void replaceAll();      //!< User has clicked the Replace All button.
    
  protected:
    void hideEvent(QHideEvent * event);
    
  private:
    Ui::SearchPanel * ui_;

  private:
    void updateButtonState();
    
  private slots:
    void on_findEdit_textChanged();
    void on_regularBox_stateChanged(int state);
};

}
}

#endif

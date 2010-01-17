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

#ifndef PLUGINS_H_
#define PLUGINS_H_

#include <QWidget>

class MainForm;
class SourceEditor;
class Simulator;

/*!
 * The EditorBuilder classes are tasked with enhancing the text editor by
 * providing additional features such as code completion or syntax highlighting.
 */
class EditorBuilder : public QObject
{
    Q_OBJECT

  public:
    EditorBuilder(QObject * parent = NULL) : QObject(parent) {}

    //! Installs additional features to the given editor.
    virtual void install(SourceEditor * editor) = 0;
    //! Uninstalls additional features from the editor.
    virtual void uninstall(SourceEditor * editor) = 0;
};

/*!
 * The SimulatorFactory classes provide simulator instances for given files.
 */
class SimulatorFactory : public QObject
{
    Q_OBJECT

  public:
    SimulatorFactory(QObject * parent = NULL) : QObject(parent) {}

    /*!
     * This function creates an instance of the simulator and loads the given
     * file into it.
     * \param fileName Initial file.
     * \param root Main window.
     */
    virtual Simulator * load(const QString & fileName, MainForm * root) = 0;
};

/*!
 * The PreferencesPage serve as a template for pages in preferences dialog.
 * \see PreferencesDialog
 */
class PreferencesPage : public QWidget
{
    Q_OBJECT

  public:
    PreferencesPage(QWidget * parent) : QWidget(parent) {}

  public slots:
    //! Reloads settings
    virtual void readSettings() = 0;
    //! Writes application settings
    virtual void writeSettings() = 0;
};

/*!
 * The AbstractPlugin class provides interface for all plugins.
 */
class AbstractPlugin
{
  public:
    virtual ~AbstractPlugin() {};

    /*!
     * This function installs the plugin to the program.
     * \param root Application's main window.
     */
    virtual void install(MainForm * root) = 0;
};

Q_DECLARE_INTERFACE(AbstractPlugin, "edu.ParaDiSe.DiVinE_IDE.AbstractPlugin/1.0")

#endif

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
#include <QDir>
#include <QPluginLoader>

#include "mainform.h"
#include "plugins.h"

void loadPlugins(MainForm * root)
{
  QDir pluginsDir;
  const char * envPlugins = getenv("DIVINE_GUI_PLUGIN_PATH");

  if(envPlugins) {
    pluginsDir = QDir(envPlugins);
  } else {
    pluginsDir = QDir(qApp->applicationDirPath());
    pluginsDir.cd("plugins");
  }

  foreach(QString fileName, pluginsDir.entryList(QDir::Files)) {
    QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));
    QObject * plugin = loader.instance();

    if (plugin) {
      AbstractPlugin * ap = qobject_cast< AbstractPlugin* >(plugin);

      if (ap)
        ap->install(root);
    } else {
      qDebug("%s", loader.errorString().toAscii().data());
    }
  }
}

int main(int argc, char * argv[])
{
  QApplication app(argc, argv);

  MainForm mf;

  // load plugins
  loadPlugins(&mf);

  mf.initialize();
  mf.show();
  return app.exec();
}

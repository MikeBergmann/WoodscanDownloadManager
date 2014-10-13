/*
  Copyright 2014 Mike Bergmann, Bones AG

  This file is part of WoodscanDownloadManager.

  WoodscanDownloadManager is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  WoodscanDownloadManager is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with WoodscanDownloadManager.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QTranslator>

#include <QApplication.h>
#include "mainwidget.h"

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  QTranslator translator;
  translator.load(a.applicationName() + "_" + QLocale::system().name(), a.applicationDirPath());
  a.installTranslator(&translator);

  QTranslator translator2;
  translator2.load("qtbase_" + QLocale::system().name(), a.applicationDirPath());
  a.installTranslator(&translator2);

  MainWidget wid;
  wid.setWindowState(Qt::WindowMinimized);
  wid.show();
  return a.exec();
}

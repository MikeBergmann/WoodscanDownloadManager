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

#ifndef MAINWIDGET_H_
#define MAINWIDGET_H_

#include <QWidget>

#include "downloadmanager.h"

namespace Ui {
  class Form;
}

class MainWidget : public QWidget {
  Q_OBJECT

public:
  explicit MainWidget(QWidget *parent = 0);

signals:
  void start(void);

private slots:
  void printText(QString qsLine);
  void progress(int percentage);
  void finished(void);

  void checkMilestone(void);

private:

  enum Modes {
    mode_none = 0,
    mode_md5,
    mode_db
  };

  Ui::Form *m_ui;
  QString m_url;
  QString m_urlParameter;
  QString m_serial;
  QString m_drive;
  Modes m_mode;
  DownloadManager *m_manager;
  QByteArray m_md5;
};

#endif // MAINWIDGET_H_


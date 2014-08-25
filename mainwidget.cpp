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

#include "mainwidget.h"
#include "ui_form.h"
#include "milestone.h"
#include <QProgressBar>
#include <QPushButton>
#include <QInputDialog>

#define NL QString("\n\r")

MainWidget::MainWidget(QWidget *parent)
: QWidget(parent)
, m_ui(new Ui::Form)
, m_url()
, m_urlParameter()
, m_serial()
, m_drive()
, m_manager(0) {
  m_ui->setupUi(this);

  m_url = "http://woodscan.bones.ch/updater.php";
  m_urlParameter = "?serial=%1&rType=%2";

  m_manager = new DownloadManager(this);
  connect(m_manager, SIGNAL(printText(QString)), this, SLOT(printText(QString)));
  connect(m_manager, SIGNAL(complete()), this, SLOT(finished()));
  connect(m_manager, SIGNAL(downloadProgress(int)), this, SLOT(progress(int)));

  // QueuedConnection because we want to finish constructur befor calling the slot.
  connect(this, SIGNAL(start()), this, SLOT(checkMilestone()), Qt::QueuedConnection);

  emit start();
}

void MainWidget::checkMilestone()
{
  m_ui->textEdit->textCursor().insertText(tr("Welcome to the Bones Woodcann Download Manager!\n\r"));

  // Get attached Milestone and check if Barcode is installed
  if(!getMilestoneSerial(0xEF8,0x312, m_serial, m_drive))
    getMilestoneSerial(0xEF8,0x212, m_serial, m_drive);

  if(m_serial.isEmpty()) {
    bool ok;
    m_serial = QInputDialog::getText(this, tr("Enter Serial"),
                                   tr("Please enter Milestone Serial Number:"), QLineEdit::Normal,
                                   QString(), &ok);
    if (ok && !m_serial.isEmpty())
      m_ui->textEdit->textCursor().insertText(tr("Milestone Serial %1, but no connected").arg(m_serial)+NL);
  } else {
      m_ui->textEdit->textCursor().insertText(tr("Milestone with Serial %1 connected as %2").arg(m_serial).arg(m_drive.at(0)));
      if(m_drive.length()>1) {
        m_ui->textEdit->textCursor().insertText(tr(" and %1").arg(m_drive.at(1)+NL));
      } else {
        m_ui->textEdit->textCursor().insertText(NL);
      }
  }

  // Download MD5 file and check if we already know this file
  m_mode = mode_md5;
  m_manager->download(m_url+m_urlParameter.arg(m_serial).arg(m_mode), &m_md5);
}

void MainWidget::printText(QString text) {
  qDebug() << text;
}

void MainWidget::progress(int percentage) {
  m_ui->textEdit->undo();
  m_ui->textEdit->textCursor().insertText(tr("Download: %1 %").arg(percentage)+NL);
}

void MainWidget::finished(void) {
  switch(m_mode) {
  case mode_md5:
    m_ui->textEdit->textCursor().insertText(tr("MD5: %1").arg(QString(m_md5))+NL);

    m_mode = mode_db;
    qDebug() << "Start:" << QTime::currentTime() << endl;
    m_manager->download(m_url+m_urlParameter.arg(m_serial).arg(m_mode));

    break;
  case mode_db:
      qDebug() << "Finished:" << QTime::currentTime() << endl;
    break;
  default:
    ;
  }
}



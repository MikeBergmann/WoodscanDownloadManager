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
#include <QFileDialog>

#include "download.h"

#define NL QString("\n\r")
#define PATH QString("/Barcode")
#define MD5FILE QString("/germany.md5")
#define DATAFILE QString("/germany.ebx")

MainWidget::MainWidget(QWidget *parent)
: QWidget(parent)
, m_ui(new Ui::Form)
, m_url()
, m_urlParameter()
, m_serial()
, m_drive()
, m_destinationPath()
, m_manager(0)
, m_md5()
, m_webdata()
{
  m_ui->setupUi(this);

  m_url = "http://woodscan.bones.ch/updater.php";
  m_urlParameter = "?serial=%1&rType=%2";

  m_manager = new DownloadManager(this);
  connect(m_manager, SIGNAL(printText(QString)), this, SLOT(debugText(QString)));
  connect(m_manager, SIGNAL(complete(Download*)), this, SLOT(finished(Download*)));
  connect(m_manager, SIGNAL(downloadProgress(int)), this, SLOT(progress(int)));

  // QueuedConnection because we want to finish constructur befor calling the slot.
  connect(this, SIGNAL(start()), this, SLOT(checkMilestone()), Qt::QueuedConnection);

  emit start();
}

void MainWidget::checkMilestone()
{
  printText(tr("Welcome to the Bones Woodcann Download Manager!") + NL);

  // Get attached Milestone and check if barcode database is installed
  if(!getMilestoneSerial(0xEF8,0x312, m_serial, m_drive))
    getMilestoneSerial(0xEF8,0x212, m_serial, m_drive);

  if(m_serial.isEmpty()) {
    bool ok;
    m_serial = QInputDialog::getText(this, tr("Enter serial"),
                                   tr("Please enter Milestone serial number:"), QLineEdit::Normal,
                                   QString(), &ok);
    if (ok && !m_serial.isEmpty()) {
      printText(tr("Milestone serial %1, device not connected").arg(m_serial) + NL);
    }
  } else {
      printText(tr("Milestone with serial %1 connected as drive %2").arg(m_serial).arg(m_drive.at(0)));
      if(m_drive.length()>1) {
        printText(tr(" and %1").arg(m_drive.at(1) + NL));
      } else {
        printText(NL);
      }
  }

  if(m_serial.isEmpty()) {
    printText(tr("No Milestone serial number. Aborting.") + NL);
    return;
  }

  if(!m_drive.isEmpty()) {
    m_destinationPath = m_drive.at(0) + ':' + PATH;
    if(!QFileInfo(m_destinationPath + MD5FILE).exists()) {
      if(m_drive.size() > 1) {
        m_destinationPath = m_drive.at(1) + ':' + PATH;
        if(!QFileInfo(m_destinationPath + MD5FILE).exists()) {
          m_destinationPath.clear();
        }
      }
    }
  }

  // Look for MD5 File
  if(m_destinationPath.isEmpty()) {
    m_destinationPath = QFileDialog::getExistingDirectory(this, tr("Please select destination directory"));
  }

  QFile md5file(m_destinationPath + MD5FILE);
  if(md5file.exists()) {
    if(md5file.open(QIODevice::ReadOnly)) {
      m_md5 = md5file.readAll();
      md5file.close();
    }
  }

  // Download MD5 file and check if we already know this file
  m_mode = mode_md5;
  m_manager->download(m_url+m_urlParameter.arg(m_serial).arg(m_mode), &m_webdata);
}

void MainWidget::printText(QString text)
{
  m_ui->textEdit->textCursor().insertText(text);
}

void MainWidget::debugText(QString text)
{
  qDebug() << text;
}

void MainWidget::progress(int percentage)
{
  if(percentage > 0 && percentage < 100) {
    m_ui->textEdit->undo();
    printText(tr("Download: %1 %").arg(percentage) + NL);
  }
}

void MainWidget::finished(Download *dl)
{
  static Download *filedl = 0;

  switch(m_mode) {
  case mode_md5:
    {
      qDebug() << "Checksum of online database:" << m_webdata << endl;
      if(m_webdata == m_md5) {
        printText(tr("You already have the newest WoodScan database.") + NL);
        return;
      }

      m_md5 = m_webdata;
      printText(tr("There is a newer WoodScan database. Starting download...") + NL);

      m_mode = mode_db;
      qDebug() << "Start:" << QTime::currentTime() << endl;
      filedl = m_manager->download(m_url+m_urlParameter.arg(m_serial).arg(m_mode));
      m_webdata.clear();
      m_manager->download(m_url+m_urlParameter.arg(m_serial).arg(3), &m_webdata);
      printText(NL);
    }
    break;
  case mode_db:
    if(dl != filedl) {
      m_ui->textEdit->undo();
      printText(tr("Generate File: %1 %").arg(QString(m_webdata)) + NL);
      if(QString(m_webdata) != "100") {
        m_webdata.clear();
        m_manager->download(m_url+m_urlParameter.arg(m_serial).arg(3), &m_webdata);
      }
    } else {
      m_mode = mode_none;
      if(dl->error()) {
        printText(QString("Download error %1").arg(dl->error()) + NL);
      } else {
        printText("Download finished, copy files..." + NL);
        QFile md5file(m_destinationPath + MD5FILE);
        if(md5file.open(QIODevice::WriteOnly)) {
          md5file.write(m_md5);
          md5file.close();
        }

        QFile::remove(m_destinationPath + DATAFILE);
        QFile::copy(dl->filename(), m_destinationPath + DATAFILE);
        printText("Finished!" + NL);
      }
    }
    break;
  default:
    ;
  }
}



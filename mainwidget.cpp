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
#include <QTime>
#include <QTimer>
#include <QMessageBox>

#include "download.h"
#include "application.h"

#define NL QString("\n\r")
#define PATH QString("Barcode")
#define MD5FILE QString("germany.md5")
#define DATAFILE QString("germany.ebx")

MainWidget::MainWidget(QWidget *parent)
: QWidget(parent)
, m_ui(new Ui::Form)
, m_url()
, m_urlParameter()
, m_serial()
, m_drives()
, m_destinationPath()
, m_manager(0)
, m_md5()
, m_webdata()
, m_checkProgress(0)
, m_filedl(0)
{
  m_ui->setupUi(this);

  m_url = "http://woodscan.bones.ch/updater.php";
  m_urlParameter = "?serial=%1&rType=%2";

  m_manager = new DownloadManager(this);
  connect(m_manager, SIGNAL(printText(QString)), this, SLOT(debugText(QString)));
  connect(m_manager, SIGNAL(complete(Download*)), this, SLOT(downloadFinished(Download*)));
  connect(m_manager, SIGNAL(failed(Download*)), this, SLOT(downloadFailed(Download*)));
  connect(m_manager, SIGNAL(downloadProgress(Download*, int)), this, SLOT(downloadProgress(Download*, int)));

  Application *app = dynamic_cast<Application*>(qApp);
  connect(app, SIGNAL(keyPressed()), this, SLOT(nextStep()));

  m_checkProgress = new QTimer(this);
  m_checkProgress->setInterval(5000);
  m_checkProgress->setSingleShot(true);

  // Timeout timer
  connect(m_checkProgress, SIGNAL(timeout()), this, SLOT(checkProgress()));

  // QueuedConnection because we want to finish constructor before calling the slot.
  connect(this, SIGNAL(start()), this, SLOT(checkMilestone()), Qt::QueuedConnection);  

  emit start();
}

void MainWidget::checkMilestone()
{
  printText(tr("Welcome to the Bones Woodscan Download Manager!") + NL);

  printText(tr("Please make sure you have a backup of you Woodscan database. This Tools will replace your old database.") + NL);

  printText(tr("Aditionally, please close all other open applications, because applications locking files on your Milestone may cause the download to fail.") + NL);

  // Get attached Milestone and check if barcode database is installed
  if(!getMilestoneSerial(0xEF8,0x312, m_serial, m_drives))
    getMilestoneSerial(0xEF8,0x212, m_serial, m_drives);

  if(m_serial.isEmpty()) {
    bool ok;
    m_serial = QInputDialog::getText(this, tr("Enter serial"),
                                   tr("No Milestone connected. Please enter Milestone serial number:"), QLineEdit::Normal,
                                   QString(), &ok);
    if (ok && !m_serial.isEmpty()) {
      printText(tr("Milestone serial %1, device not connected.").arg(m_serial) + NL);
    }
  } else {
      printText(tr("Milestone with serial %1 connected as drive %2").arg(m_serial).arg(m_drives.at(0)));
      if(m_drives.length()>1) {
        printText(tr(" and %1.").arg(m_drives.at(1)) + NL);
      } else {
        printText(NL);
      }
  }

  if(m_serial.isEmpty()) {
    printText(tr("No Milestone serial number. Aborting.") + NL);
    return;
  }

  if(!m_drives.isEmpty()) {
    m_destinationPath = QString(m_drives.at(0)) + ":/" + PATH;
    if(!QFileInfo(m_destinationPath + "/" + MD5FILE).exists()) {
      if(m_drives.size() > 1) {
        m_destinationPath = QString(m_drives.at(1)) + ":/" + PATH;
        if(!QFileInfo(m_destinationPath + "/" + MD5FILE).exists()) {
          m_destinationPath.clear();
        }
      }
    }
  }

  // Look for MD5 File
  if(m_destinationPath.isEmpty()) {

    if(availableDiskSpace(QString(m_drives.at(0)) + ":/") > availableDiskSpace(QString(m_drives.at(1)) + ":/")) {
      m_destinationPath =QString( m_drives.at(0)) + ":/" + PATH;
    } else {
      m_destinationPath = QString(m_drives.at(1)) + ":/" + PATH;
    }

    QMessageBox::StandardButton button;
    button = QMessageBox::question(this, tr("No existing database found"),
                                   tr("No existing Woodscan database found, do you want to select the directory manually? If not I will choose a proper one for you."),
                                   QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
    if(button == QMessageBox::Yes) {
      m_destinationPath = QFileDialog::getExistingDirectory(this, tr("Please select destination directory"), m_destinationPath);
      if(m_destinationPath.isEmpty()) {
        printText(tr("No destination directory. Aborting.") + NL);
        return;
      }
    }

    QDir dir(m_destinationPath);
    if(!dir.exists()) {
      if(!dir.mkpath(m_destinationPath)) {
        printText(tr("No destination directory. Aborting.") + NL);
        return;
      }
    }
  }

  QFile md5file(m_destinationPath + "/" + MD5FILE);
  if(md5file.exists()) {
    if(md5file.open(QIODevice::ReadOnly)) {
      m_md5 = md5file.readAll();
      md5file.close();
    }
  }

  m_mode = mode_md5;
  printText(tr("I'm now going online to check if a new WoodScan database is available. Please press a key to continue.") + NL);
}

void MainWidget::checkProgress()
{
  m_webdata.clear();
  m_manager->download(m_url+m_urlParameter.arg(m_serial).arg(3), &m_webdata);
}

void MainWidget::printText(QString text)
{
  m_ui->textEdit->textCursor().insertText(text);
}

void MainWidget::debugText(QString text)
{
  qDebug() << text;
}

void MainWidget::downloadProgress(Download *dl, int percentage)
{
  static int msecs = 0;    
  static bool firstrun = true;

  if(dl == m_filedl) {
    if(firstrun) {
      firstrun = false;
      if(QFile::exists(m_destinationPath + "/" + DATAFILE)) {
        QFileInfo info(m_destinationPath + "/" + DATAFILE);
        if(dl->filesize()) {
          if(dl->filesize() > info.size()) {
            if(availableDiskSpace(m_destinationPath) < (dl->filesize() - info.size())) {
              dl->stop();
              m_ui->textEdit->clear();
              printText(tr("Not enough free disk space in %1. Aborting.").arg(m_destinationPath) + NL);
              return;
            }
          }
        }
        QFile::remove(m_destinationPath + "/" + DATAFILE);
      }
    }

    if(percentage > 0 && percentage < 100) {
      if(msecs == 0 || QTime::currentTime().msecsSinceStartOfDay()-msecs > 10000) {
        msecs = QTime::currentTime().msecsSinceStartOfDay();
        m_ui->textEdit->clear();
        printText(tr("Downloading: %1 %").arg(percentage) + NL);
      }
    }
  }
}

void MainWidget::downloadFinished(Download *dl)
{
  static int conversionProgress = 0;

  switch(m_mode) {
  case mode_md5:
    {
      qDebug() << "Checksum of online database:" << m_webdata << endl;
      if(m_webdata == m_md5) {
        printText(tr("You already have the newest WoodScan database.") + NL);
        m_mode = mode_none;
        return;
      }

      m_md5 = m_webdata;
      printText(tr("There is a newer WoodScan database. Starting download...") + NL);

      m_mode = mode_db;
      qDebug() << "Start:" << QTime::currentTime() << endl;
      m_filedl = m_manager->download(m_url+m_urlParameter.arg(m_serial).arg(m_mode), m_destinationPath);
      m_checkProgress->start();
      printText(NL);
    }
    break;
  case mode_db:
    if(dl != m_filedl) {
      if(conversionProgress < QString(m_webdata).toInt()) {
        m_filedl->timerStart(); // We got a progress of database generation, so restart timer.
      }

      conversionProgress = QString(m_webdata).toInt();
      if(conversionProgress != 100) {
        if(conversionProgress > 0) {
          m_ui->textEdit->clear();
          printText(tr("Generating database file: %1 %").arg(QString(m_webdata)) + NL);
        }

        m_checkProgress->start();
      }
    } else {
      m_mode = mode_none;
      QFile file(dl->filename());
      if(!file.rename(m_destinationPath + "/" + DATAFILE)) {
        m_ui->textEdit->clear();
        printText(tr("Rename failed (%1 %2)!").arg(file.errorString()).arg(file.error()) + NL);
      } else {
        m_ui->textEdit->clear();
        printText(tr("Download finished, good bye.") + NL);
        QFile md5file(m_destinationPath + "/" + MD5FILE);
        if(md5file.open(QIODevice::WriteOnly)) {
          md5file.write(m_md5);
          md5file.close();
        }
      }
    }
    break;
  default:
    ;
    }
}

void MainWidget::downloadFailed(Download *dl)
{
  m_ui->textEdit->clear();
  printText(tr("Download error!").arg(dl->error()) + NL);
  printText(tr("Please check if you are eligible to download a new database for your Milestone.") + NL);
  printText(tr("Additionally please make sure the destination directory is not write protected and has enough free space.") + NL);
  printText(tr("Please restart this application to try again.") + NL);
}

void MainWidget::nextStep()
{
  switch(m_mode) {
  case mode_md5:
    // Download MD5 file and check if we already know this file
    m_manager->download(m_url+m_urlParameter.arg(m_serial).arg(m_mode), &m_webdata);
    m_ui->textEdit->clear();
    break;
  default:
      ;
  }
}


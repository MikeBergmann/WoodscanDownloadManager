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
, m_checkProgress(0)
{
  m_ui->setupUi(this);

  m_url = "http://woodscan.bones.ch/updater.php";
  m_urlParameter = "?serial=%1&rType=%2";

  m_manager = new DownloadManager(this);
  connect(m_manager, SIGNAL(printText(QString)), this, SLOT(debugText(QString)));
  connect(m_manager, SIGNAL(complete(Download*)), this, SLOT(downloadFinished(Download*)));
  connect(m_manager, SIGNAL(failed(Download*)), this, SLOT(downloadFailed(Download*)));
  connect(m_manager, SIGNAL(downloadProgress(int)), this, SLOT(downloadProgress(int)));

  Application *app = dynamic_cast<Application*>(qApp);
  connect(app, SIGNAL(keyPressed()), this, SLOT(nextStep()));

  m_checkProgress = new QTimer(this);
  m_checkProgress->setInterval(5000);
  m_checkProgress->setSingleShot(true);

  // Timeout timer
  connect(m_checkProgress, SIGNAL(timeout()), this, SLOT(checkProgress()));

  // QueuedConnection because we want to finish constructur befor calling the slot.
  connect(this, SIGNAL(start()), this, SLOT(checkMilestone()), Qt::QueuedConnection);  

  emit start();
}

void MainWidget::checkMilestone()
{
  printText(tr("Welcome to the Bones Woodcann Download Manager!") + NL);

  printText(tr("Please make sure you have a backup of you Woodscan database. This Tools will replace your old database.") + NL);

  // Get attached Milestone and check if barcode database is installed
  if(!getMilestoneSerial(0xEF8,0x312, m_serial, m_drive))
    getMilestoneSerial(0xEF8,0x212, m_serial, m_drive);

  if(m_serial.isEmpty()) {
    bool ok;
    m_serial = QInputDialog::getText(this, tr("Enter serial"),
                                   tr("No Milestone connected. Please enter Milestone serial number:"), QLineEdit::Normal,
                                   QString(), &ok);
    if (ok && !m_serial.isEmpty()) {
      printText(tr("Milestone serial %1, device not connected.").arg(m_serial) + NL);
    }
  } else {
      printText(tr("Milestone with serial %1 connected as drive %2").arg(m_serial).arg(m_drive.at(0)));
      if(m_drive.length()>1) {
        printText(tr(" and %1.").arg(m_drive.at(1)) + NL);
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
    QMessageBox::StandardButton button;
    button = QMessageBox::question(this, tr("No existing database found"), tr("No existion Woodscan database found, do you want to select the directory manually?"));
    if(button == QMessageBox::Yes) {
      m_destinationPath = QFileDialog::getExistingDirectory(this, tr("Please select destination directory"));
    } else {
      printText(tr("No destination directory. Aborting.") + NL);
      return;
    }
  }

  QFile md5file(m_destinationPath + MD5FILE);
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

void MainWidget::downloadProgress(int percentage)
{
  static int msecs = 0;

  if(percentage > 0 && percentage < 100) {
    if(msecs == 0 || QTime::currentTime().msecsSinceStartOfDay()-msecs > 10000) {
      msecs = QTime::currentTime().msecsSinceStartOfDay();
      m_ui->textEdit->clear();
      printText(tr("Downloading: %1 %").arg(percentage) + NL);
    }
  }
}

void MainWidget::downloadFinished(Download *dl)
{
  static Download *filedl = 0;
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
      QFile::remove(m_destinationPath + DATAFILE);

      m_mode = mode_db;
      qDebug() << "Start:" << QTime::currentTime() << endl;
      filedl = m_manager->download(m_url+m_urlParameter.arg(m_serial).arg(m_mode), m_destinationPath);
      m_checkProgress->start();
      printText(NL);
    }
    break;
  case mode_db:
    if(dl != filedl) {
      if(conversionProgress < QString(m_webdata).toInt()) {
        filedl->timerStart(); // We got a progress of database generation, so restart timer.
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
      m_ui->textEdit->clear();
      printText("Download finished, good bye" + NL);
      QFile md5file(m_destinationPath + MD5FILE);
      if(md5file.open(QIODevice::WriteOnly)) {
        md5file.write(m_md5);
        md5file.close();
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
  printText(QString("Download error %1").arg(dl->error()) + NL);
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


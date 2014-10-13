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
#include <QStandardPaths>
#include <QSettings>
#include <QProgressDialog>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QApplication>

#include "download.h"

#define NL QString("\n")
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
, m_progress(0)
, m_trayIcon(0)
, m_trayIconMenu(0)
, m_quitAction(0)
, m_retry(3)
{
  m_ui->setupUi(this);

  m_url = "http://woodscan.bones.ch/updater.php";
  m_urlParameter = "?serial=%1&rType=%2";

  m_manager = new DownloadManager(this);

  m_checkProgress = new QTimer(this);
  m_checkProgress->setInterval(5000);
  m_checkProgress->setSingleShot(true);

  // Timeout timer
  connect(m_checkProgress, SIGNAL(timeout()), this, SLOT(checkProgress()));

  // QueuedConnection because we want to finish constructor before calling the slot.
  connect(this, SIGNAL(start()), this, SLOT(checkMilestone()), Qt::QueuedConnection);  

  m_progress = new QProgressDialog(tr("Download..."),tr("Cancel"),0,100,this);
  m_progress->setWindowModality(Qt::WindowModal);
  m_progress->setMinimumDuration(0);

  createActions();
  createTrayIcon();

  m_trayIcon->setIcon(QIcon(":/Bones/WoodscanDM.ico"));
  m_trayIcon->show();

  m_ui->label_2->setText(QString("WoodscanDM Version 1.0 (") + __DATE__ + " " + __TIME__ +")");

  setVisible(false);

  emit start();
}

void MainWidget::createActions()
{
  m_quitAction = new QAction(tr("&Quit"), this);
  connect(m_quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
}

void MainWidget::stop(Download *dl)
{
  disconnect(m_manager, SIGNAL(complete(Download*)), this, SLOT(downloadFinished(Download*)));
  disconnect(m_manager, SIGNAL(failed(Download*)), this, SLOT(downloadFailed(Download*)));
  disconnect(m_manager, SIGNAL(downloadProgress(Download*, int)), this, SLOT(downloadProgress(Download*, int)));

  if(dl) {
    dl->stop();
  }
}

Download* MainWidget::start(QUrl url, QByteArray *destination)
{
  connect(m_manager, SIGNAL(complete(Download*)), this, SLOT(downloadFinished(Download*)));
  connect(m_manager, SIGNAL(failed(Download*)), this, SLOT(downloadFailed(Download*)));
  connect(m_manager, SIGNAL(downloadProgress(Download*, int)), this, SLOT(downloadProgress(Download*, int)));

  return m_manager->download(url, destination);
}

Download* MainWidget::start(QUrl url, QString &destination)
{
  connect(m_manager, SIGNAL(complete(Download*)), this, SLOT(downloadFinished(Download*)));
  connect(m_manager, SIGNAL(failed(Download*)), this, SLOT(downloadFailed(Download*)));
  connect(m_manager, SIGNAL(downloadProgress(Download*, int)), this, SLOT(downloadProgress(Download*, int)));

  return m_manager->download(url, destination);
}

void MainWidget::critical(QString &text, Download *dl)
{
  stop(dl);
  QMessageBox::critical(this,"",text);
  qApp->quit();
}

void MainWidget::createTrayIcon()
{
  m_trayIconMenu = new QMenu(this);
  m_trayIconMenu->addAction(m_quitAction);

  m_trayIcon = new QSystemTrayIcon(this);
  m_trayIcon->setContextMenu(m_trayIconMenu);
}

void MainWidget::checkMilestone()
{
  QString text;

  text += tr("Welcome to the Bones Woodscan Download Manager!") + NL + NL;
  text += tr("Please make sure you have a backup of you Woodscan database. This Tools will replace your old database.") + NL + NL;
  text += tr("Aditionally, please close all other open applications, because applications locking files on your Milestone may cause the download to fail.") + NL + NL;

  // Get attached Milestone and check if barcode database is installed
  if(!getMilestoneSerial(0xEF8,0x312, m_serial, m_drives))
    getMilestoneSerial(0xEF8,0x212, m_serial, m_drives);

  if(m_serial.isEmpty()) {
    bool ok;
    m_serial = QInputDialog::getText(this, tr("Enter serial"),
                                   tr("No Milestone connected. Please enter Milestone serial number:"), QLineEdit::Normal,
                                   QString(), &ok);
    if (ok && !m_serial.isEmpty()) {
      text += tr("Milestone serial %1, device not connected.").arg(m_serial) + NL;
    }
  } else {
      text += tr("Milestone with serial %1 connected as drive %2").arg(m_serial).arg(m_drives.at(0));
      if(m_drives.length()>1) {
        text += tr(" and %1.").arg(m_drives.at(1)) + NL + NL;
      } else {
        text += NL;
      }
  }

  if(m_serial.isEmpty()) {
    text = tr("No Milestone serial number. Aborting.") + NL;
    critical(text, 0);
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
      } else {
        m_destinationPath.clear();
      }
    }
  }

  // Look for MD5 File
  if(m_destinationPath.isEmpty()) {

    if(m_drives.length() > 1 && availableDiskSpace(QString(m_drives.at(0)) + ":/") < availableDiskSpace(QString(m_drives.at(1)) + ":/")) {
      m_destinationPath =QString( m_drives.at(1)) + ":/" + PATH;
    } else if(!m_drives.isEmpty()) {
      m_destinationPath = QString(m_drives.at(0)) + ":/" + PATH;
    } else {
      m_destinationPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    QMessageBox::StandardButton button;
    QMessageBox box(this);
    box.setText(tr("No existing Woodscan database found, do you want to select the directory manually? If not I will choose a proper one for you."));
    box.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
    box.button(QMessageBox::Yes)->setText(tr("Choose myself"));
    box.button(QMessageBox::No)->setText(tr("Let the tool decide"));
    box.setIcon(QMessageBox::Question);
    box.setModal(true);
    box.setDefaultButton(QMessageBox::No);

    box.exec();
    button = box.clickedButton() == box.button(QMessageBox::Yes) ? QMessageBox::Yes : QMessageBox::No;

    if(button == QMessageBox::Yes) {
      QString path = QFileDialog::getExistingDirectory(this, tr("Please select destination directory"), m_destinationPath);
      if(!m_destinationPath.isEmpty()) {
        m_destinationPath = path;
      }
    }

    QDir dir(m_destinationPath);
    if(!dir.exists()) {
      if(!dir.mkpath(m_destinationPath)) {
        text += tr("No destination directory. Aborting.");
        critical(text, 0);
        return;
      }
    }
  }

  QDir dir(m_destinationPath);
  dir.setNameFilters(QStringList("*.part"));
  dir.setFilter(QDir::Files);
  foreach(QString dirFile, dir.entryList()) {
      dir.remove(dirFile);
  }

  QSettings md5file(m_destinationPath + "/" + MD5FILE, QSettings::IniFormat);
  if(md5file.contains("Serial")) {
    if(md5file.value("Serial").toString() == m_serial) {
      if(md5file.contains("MD5")) {
        m_md5 = md5file.value("MD5").toByteArray();
      }
    }
  }

  m_mode = mode_md5;

  text += tr("I'm now going online to check if a new WoodScan database is available. Please confirm to continue.") + NL;

  QMessageBox::StandardButton ret = QMessageBox::information(this,"",text,QMessageBox::Ok, QMessageBox::Cancel);
  if(ret == QMessageBox::Ok) {
    nextStep();
  } else {
    qApp->quit();
  }
}

void MainWidget::checkProgress()
{
  m_webdata.clear();
  start(m_url+m_urlParameter.arg(m_serial).arg(3), &m_webdata);
}

void MainWidget::debugText(QString text)
{
  qDebug() << text;
}

void MainWidget::downloadProgress(Download *dl, int percentage)
{
  QString text;
  static bool firstrun = true;

  if(dl == m_filedl) {
    if(firstrun) {
      firstrun = false;
      if(QFile::exists(m_destinationPath + "/" + DATAFILE)) {
        QFileInfo info(m_destinationPath + "/" + DATAFILE);
        if(dl->filesize()) {
          if(dl->filesize() > info.size()) {
            if(availableDiskSpace(m_destinationPath) < (dl->filesize() - info.size())) {
              text = tr("Not enough free disk space in %1. Aborting.").arg(m_destinationPath) + NL;
              critical(text, dl);
              return;
            }
          }
        }
        QFile::remove(m_destinationPath + "/" + DATAFILE);
      } else {
        if(dl->filesize()) {
          if(availableDiskSpace(m_destinationPath) < dl->filesize()) {
            text = tr("Not enough free disk space in %1. Aborting.").arg(m_destinationPath) + NL;
            critical(text, dl);
            return;
          }
        }
      }
    }

    if(percentage >= 0 && percentage < 100) {      
      m_progress->setLabelText(tr("Download..."));
      m_progress->setValue(percentage);
      if(m_progress->wasCanceled()) {
        m_manager->pause(dl);
        qApp->quit();
        return;
      }
    }
  }
}

void MainWidget::downloadFinished(Download *dl)
{
  static int conversionProgress = 0;
  QString text;

  switch(m_mode) {
  case mode_md5:
    {
      qDebug() << "Checksum of online database:" << m_webdata << endl;
      if(m_webdata.isEmpty()) {
        text = tr("Can not connect to Bones Webservice, please try again later.") + NL;
        QMessageBox::critical(this,"",text);
        qApp->quit();
        return;
      }
      if(m_webdata == m_md5) {
        text = tr("You already have the newest WoodScan database.") + NL;
        QMessageBox::information(this,"",text);
        qApp->quit();
        return;
      }

      m_md5 = m_webdata;

      m_mode = mode_db;
      qDebug() << "Start:" << QTime::currentTime() << endl;
      m_filedl = start(m_url+m_urlParameter.arg(m_serial).arg(m_mode), m_destinationPath);
      m_checkProgress->start();
      m_progress->setValue(0);
    }
    break;
  case mode_db:

    if(m_progress->wasCanceled()) {
      stop(dl);
      qApp->quit();
      return;
    }

    if(dl != m_filedl) {
      if(conversionProgress < QString(m_webdata).toInt()) {
        m_filedl->timerStart(); // We got a progress of database generation, so restart timer.
      }

      conversionProgress = QString(m_webdata).toInt();
      if(conversionProgress != 100) {
        if(conversionProgress > 0) {
          m_progress->setLabelText(tr("Generating database file..."));
          m_progress->setValue(QString(m_webdata).toInt());
        }

        m_checkProgress->start();
      }
    } else {
      m_mode = mode_none;
      QFile file(dl->filename());
      if(!file.rename(m_destinationPath + "/" + DATAFILE)) {
        text = tr("Rename failed (%1 %2)!").arg(file.errorString()).arg(file.error()) + NL;
        QMessageBox::critical(this,"",text);
        qApp->quit();
        return;
      } else {
        m_progress->setValue(m_progress->maximum());
        QSettings md5file(m_destinationPath + "/" + MD5FILE, QSettings::IniFormat);
        md5file.setValue("Serial",m_serial);
        md5file.setValue("MD5", m_md5);
        md5file.sync();

        text = tr("Download finished, good bye.") + NL;
        text += "(" + m_destinationPath + "/" + DATAFILE + ")";
        QMessageBox::information(this,"",text);
        qApp->quit();
        return;
      }
    }
    break;
  default:
    ;
    }
}

void MainWidget::downloadFailed(Download *dl)
{
  qDebug() << "Error:" << dl->error();

  // Sometimes the file is not closed fast enough on the server after creation, simply retry...
  if(m_mode == mode_db && dl->error() == QNetworkReply::InternalServerError && m_retry > 0) {
    stop(dl);
    m_retry--;
    m_filedl = start(m_url+m_urlParameter.arg(m_serial).arg(m_mode), m_destinationPath);
    m_checkProgress->start();
    m_progress->setValue(0);
  }

  QString text;

  text = tr("Download error!") + NL;
  text += tr("Please check if you are eligible to download a new database for your Milestone.") + NL;
  text += tr("Additionally please make sure the destination directory is not write protected and has enough free space.") + NL;
  text += tr("Please restart this application to try again.") + NL;
  critical(text, dl);
  return;
}

void MainWidget::nextStep()
{
  switch(m_mode) {
  case mode_md5:
    // Download MD5 file and check if we already know this file    
    start(m_url+m_urlParameter.arg(m_serial).arg(m_mode), &m_webdata);
    break;
  default:
      ;
  }
}


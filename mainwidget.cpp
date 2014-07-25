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

MainWidget::MainWidget(QWidget *parent)
  : QWidget(parent)
  , m_ui(new Ui::Form)
  , m_manager(0) {
  m_ui->setupUi(this);

  m_ui->url->setText("http://jpsoft.com/downloads/v16/tcmd.exe");

  m_manager = new DownloadManager(this);
  connect(m_manager, SIGNAL(printText(QString)), this, SLOT(printText(QString)));
  connect(m_manager, SIGNAL(complete()), this, SLOT(finished()));
  connect(m_manager, SIGNAL(downloadProgress(int)), this, SLOT(progress(int)));

  QString serial;
  QString drive;
  if(!getMilestoneSerial(0xEF8,0x312, serial, drive))
    getMilestoneSerial(0xEF8,0x212, serial, drive);

  if(!serial.isEmpty())
    printText(QString("Milestone with Serial %1 connected as %2").arg(serial).arg(drive));
}


void MainWidget::on_download_clicked(void) {
  m_ui->textEdit->clear();
  QUrl url(m_ui->url->text());
  m_manager->download(url);
  m_ui->download->setEnabled(false);
  m_ui->pause->setEnabled(true);
}


void MainWidget::on_pause_clicked(void) {
  m_manager->pause();
  m_ui->pause->setEnabled(false);
  m_ui->resume->setEnabled(true);
}


void MainWidget::on_resume_clicked(void) {
  m_manager->resume();
  m_ui->pause->setEnabled(true);
  m_ui->resume->setEnabled(false);
}


void MainWidget::printText(QString text) {
  m_ui->textEdit->insertPlainText(text);
}


void MainWidget::progress(int percentage) {
  m_ui->progressBar->setValue(percentage);
}


void MainWidget::finished(void) {
  m_ui->download->setEnabled(true);
  m_ui->pause->setEnabled(false);
  m_ui->resume->setEnabled(false);
}


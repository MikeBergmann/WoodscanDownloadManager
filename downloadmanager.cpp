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

#include "downloadmanager.h"

#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QTimer>

DownloadManager::DownloadManager(QObject *parent)
  : QObject(parent)
  , m_fileName()
  , m_file(0)
  , m_manager(0)
  , m_request()
  , m_reply(0)
  , m_hostSupportsRanges(false)
  , m_totalSize(0)
  , m_downloadSize(0)
  , m_pausedSize(0)
  , m_timer(0) {
  m_manager = new QNetworkAccessManager(this);
  m_timer = new QTimer(this);
  m_timer->setInterval(5000);
  m_timer->setSingleShot(true);
  connect(m_timer, SIGNAL(timeout()), this, SLOT(timeout()));
}


DownloadManager::~DownloadManager(void) {
  try {
    if(m_reply) {
      pause();
    }
    delete m_request;
  } catch(...) {
    qDebug() << "exception within ~DownloadManager(void)" << endl;
  }
}


void DownloadManager::download(QUrl url) {
  m_fileName = QFileInfo(url.toString()).fileName();
//  m_fileName = "test.txt";
  m_downloadSize = 0;
  m_pausedSize = 0;

  m_request = new QNetworkRequest(url);

  // Request to obtain the network headers
  m_reply = m_manager->head(*m_request);
  connect(m_reply, SIGNAL(finished()), this, SLOT(gotHeader()));
  connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(gotError(QNetworkReply::NetworkError)));

  // Start timeout
  m_timer->start();
}


void DownloadManager::pause(void) {
  if(m_reply == 0) {
    return;
  }
  m_timer->stop();
  disconnect(m_reply, SIGNAL(finished()), this, SLOT(finished()));
  disconnect(m_reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));
  disconnect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(gotError(QNetworkReply::NetworkError)));

  m_reply->abort();
  m_reply->deleteLater();
  m_reply = 0;
  m_file->flush();
  m_pausedSize = m_downloadSize;
  m_downloadSize = 0;
}


void DownloadManager::resume(void) {
  download();
}


void DownloadManager::download(void) {
  if(m_hostSupportsRanges) {
    QByteArray rangeHeaderValue = "bytes=" + QByteArray::number(m_pausedSize) + "-";
    if(m_totalSize > 0) {
      rangeHeaderValue += QByteArray::number(m_totalSize);
    }
    m_request->setRawHeader("Range", rangeHeaderValue);
  }

  // Download file
  m_reply = m_manager->get(*m_request);
  connect(m_reply, SIGNAL(finished()), this, SLOT(finished()));
  connect(m_reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));
  connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(gotError(QNetworkReply::NetworkError)));

  // Start timeout
  m_timer->start();
}


void DownloadManager::gotHeader(void) {
  m_timer->stop();
  m_hostSupportsRanges = false;

  QList<QByteArray> rawHeaderList = m_reply->rawHeaderList();
  foreach(QByteArray header, rawHeaderList) {
    QString headerLine = QString(header) + ": " + m_reply->rawHeader(header);
    emit printText(headerLine+"\n");
  }

  if(m_reply->hasRawHeader("Accept-Ranges")) {
    QString acceptRanges = m_reply->rawHeader("Accept-Ranges");
    m_hostSupportsRanges = (acceptRanges.compare("bytes", Qt::CaseInsensitive) == 0);
    qDebug() << "Accept-Ranges = " << acceptRanges << m_hostSupportsRanges;
  }

  m_totalSize = m_reply->header(QNetworkRequest::ContentLengthHeader).toInt();
  m_reply->deleteLater();
  m_reply = 0;

  m_request->setRawHeader("Connection", "Keep-Alive");
  m_request->setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
  m_file = new QFile(m_fileName + ".part");
  if(!m_hostSupportsRanges) {
    m_file->remove();
  }
  m_file->open(QIODevice::ReadWrite | QIODevice::Append);

  m_pausedSize = m_file->size();
  download();
}


void DownloadManager::finished(void) {
  m_timer->stop();
  m_reply->deleteLater();
  m_reply = 0;
  m_file->close();
  QFile::remove(m_fileName);
  m_file->rename(m_fileName + ".part", m_fileName);
  delete m_file;
  m_file = 0;
  emit complete();
}


void DownloadManager::downloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
  m_timer->stop();
  m_downloadSize = m_pausedSize + bytesReceived;
  qDebug() << "Download Progress: Received=" << m_downloadSize << ": Total=" << m_pausedSize + bytesTotal;

  m_file->write(m_reply->readAll());
  int percentage = static_cast<int>((static_cast<float>(m_pausedSize + bytesReceived) * 100.0) / static_cast<float>(m_pausedSize + bytesTotal));
  qDebug() << percentage;
  emit downloadProgress(percentage);

  m_timer->start();
}


void DownloadManager::gotError(QNetworkReply::NetworkError errorcode) {
  qDebug() << __FUNCTION__ << "(" << errorcode << ")";
  m_reply->deleteLater();
}


void DownloadManager::timeout(void) {
  qDebug() << __FUNCTION__;
  m_reply->deleteLater();
}


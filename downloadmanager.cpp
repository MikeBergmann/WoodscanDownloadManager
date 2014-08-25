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
#include <QAuthenticator>

DownloadManager::DownloadManager(QObject *parent)
: QObject(parent)
, m_fileName()
, m_file(0)
, m_manager(0)
, m_request(0)
, m_reply(0)
, m_hostSupportsRanges(false)
, m_totalSize(0)
, m_downloadSize(0)
, m_pausedSize(0)
, m_timer(0) {
  m_manager = new QNetworkAccessManager(this);
  connect(m_manager, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)), this, SLOT(authenticationRequired(QNetworkReply*,QAuthenticator*)));

  // Timeout timer
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

void DownloadManager::downloadRequest(QUrl url) {
  delete m_request;
  m_request = new QNetworkRequest();
  m_request->setUrl(url);

  m_downloadSize = 0;
  m_pausedSize = 0;

  // Request to obtain the network headers
  m_reply = m_manager->head(*m_request);
  connect(m_reply, SIGNAL(finished()), this, SLOT(gotHeader()));
  connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(gotError(QNetworkReply::NetworkError)));

  // Start timeout
//  m_timer->start();
}

void DownloadManager::download(QUrl url) {
  downloadRequest(url);
}

void DownloadManager::download(QUrl url, QByteArray *destination) {
  m_stream = new QDataStream(destination,QIODevice::WriteOnly);
  downloadRequest(url);
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
//  m_timer->start();
}

void DownloadManager::gotHeader(void) {
  m_timer->stop();
  m_hostSupportsRanges = false;
  m_totalSize = 0;

  QList<QByteArray> rawHeaderList = m_reply->rawHeaderList();
  foreach(QByteArray header, rawHeaderList) {
    QString headerLine = QString(header) + ": " + m_reply->rawHeader(header);
    emit printText(headerLine);
  }

  if(m_reply->hasRawHeader("Accept-Ranges")) {
    QString acceptRanges = m_reply->rawHeader("Accept-Ranges");
    m_hostSupportsRanges = (acceptRanges.compare("bytes", Qt::CaseInsensitive) == 0);
    qDebug() << "Accept-Ranges = " << acceptRanges << m_hostSupportsRanges;
  }

  // Check if we get a relocation, this will happen if we request a download by a php script.
  QString location;
  if(m_reply->hasRawHeader("Location")) {
    location = m_reply->rawHeader("Location");
  }

  QString disposition;
  if(m_reply->hasRawHeader("Content-Disposition")) {
    disposition = m_reply->rawHeader("Content-Disposition");
  }

  m_totalSize = m_reply->header(QNetworkRequest::ContentLengthHeader).toInt();
  m_reply->deleteLater();
  m_reply = 0;

  if(!location.isEmpty()) {
     // We got a new location of the file, reissue download request.
    QUrl url = m_request->url().scheme()+"://"+m_request->url().host()+"/"+location;
    downloadRequest(url);
    return;
  }

  m_request->setRawHeader("Connection", "Keep-Alive");
  m_request->setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

  if(!m_stream) {
    if(!disposition.isEmpty()) {
      m_fileName = QString(disposition.split("filename=").at(1)).remove("\"");
    } else {
      m_fileName = "temp.bin";
    }

    m_file = new QFile(m_fileName + ".part");
    if(!m_hostSupportsRanges) {
      m_file->remove();
    }
    m_file->open(QIODevice::ReadWrite | QIODevice::Append);
    m_stream = new QDataStream(m_file);
    m_pausedSize = m_file->size();
  }

  download();
}

void DownloadManager::finished(void) {
  m_timer->stop();
  m_reply->deleteLater();
  m_reply = 0;

  if(m_file) {
    m_file->close();
    QFile::remove(m_fileName);
    m_file->rename(m_fileName + ".part", m_fileName);
    delete m_file;
    m_file = 0;
  }

  delete m_stream;
  m_stream = 0;

  emit complete();
}

void DownloadManager::downloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
  m_timer->stop();
  m_downloadSize = m_pausedSize + bytesReceived;
  qDebug() << QTime::currentTime() << "Download Progress: Received=" << m_downloadSize << ": Total=" << m_pausedSize + bytesTotal;

  QByteArray replyData = m_reply->readAll();

  // Do not stream to QDataStream because the stream makes some sort of
  // data encoding, use writeRawData instead.
  m_stream->writeRawData(replyData.data(),replyData.size());

  int percentage = static_cast<int>((static_cast<float>(m_pausedSize + bytesReceived) * 100.0) / static_cast<float>(m_pausedSize + bytesTotal));
  qDebug() << percentage;
  emit downloadProgress(percentage);

  m_timer->start();
}

void DownloadManager::gotError(QNetworkReply::NetworkError errorcode) {
  qDebug() << __FUNCTION__ << "(" << errorcode << ")";
  m_reply->deleteLater();
}

void DownloadManager::authenticationRequired(QNetworkReply */*reply*/, QAuthenticator *auth)
{
  // If we try to download from a site with authentication required we will fill the credentials here.
  auth->setUser("");
  auth->setPassword("");
}

void DownloadManager::timeout(void) {
  qDebug() << __FUNCTION__;
  m_reply->deleteLater();
}


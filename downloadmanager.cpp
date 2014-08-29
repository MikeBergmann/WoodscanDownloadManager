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
#include "download.h"

#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QTimer>
#include <QAuthenticator>

#define RETRYCNT 10

DownloadManager::DownloadManager(QObject *parent)
  : QObject(parent)
  , m_manager(0)
  , m_downloads()
{
  m_manager = new QNetworkAccessManager(this);
  connect(m_manager, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)), this, SLOT(authenticationRequired(QNetworkReply*,QAuthenticator*)));
}

DownloadManager::~DownloadManager(void)
{
  try {
    QHashIterator<QNetworkReply*, Download*> i(m_downloads);
    while (i.hasNext()) {
      i.next();
      delete i.value();
    }
  } catch(...) {
    qDebug() << "exception within ~DownloadManager(void)" << endl;
  }
}

void DownloadManager::downloadRequest(Download *dl)
{
  // Request to obtain the network headers
  dl->m_reply = m_manager->head(*(dl->m_request));
  connect(dl->m_reply, SIGNAL(finished()), this, SLOT(gotHeader()));
  connect(dl->m_reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(gotError(QNetworkReply::NetworkError)));

  // Timeout timer
  connect(dl, SIGNAL(timeout(QNetworkReply*)), this, SLOT(timeout(QNetworkReply*)));

  m_downloads.insert(dl->m_reply,dl);

  // Start timeout
//  dl->timerStart();
}

Download* DownloadManager::download(QUrl url)
{
  Download *dl = new Download(url);
  downloadRequest(dl);
  return dl;
}

Download* DownloadManager::download(QUrl url, QByteArray *destination)
{
  Download *dl = new Download(url, new QDataStream(destination,QIODevice::WriteOnly));
  downloadRequest(dl);
  return dl;
}

void DownloadManager::pause(Download *dl)
{
  disconnect(dl->m_reply, SIGNAL(finished()), this, SLOT(finished()));
  disconnect(dl->m_reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));
  disconnect(dl->m_reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(gotError(QNetworkReply::NetworkError)));

  dl->pause();
}

void DownloadManager::resume(Download *dl)
{
  doDownload(dl);
}

void DownloadManager::doDownload(Download *dl)
{
  dl->fillRequestHeader();

  // Download file
  if(dl->m_reply) {
    m_downloads.remove(dl->m_reply);
    dl->m_reply->deleteLater();
  }

  dl->m_reply = m_manager->get(*(dl->m_request));
  m_downloads.insert(dl->m_reply, dl);

  connect(dl->m_reply, SIGNAL(finished()), this, SLOT(finished()));
  connect(dl->m_reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));
  connect(dl->m_reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(gotError(QNetworkReply::NetworkError)));

  // Start timeout
//  dl->timerStart();
}

void DownloadManager::gotHeader(void)
{
  Download *dl = m_downloads.value(dynamic_cast<QNetworkReply*>(QObject::sender()));

  dl->timerStop();

  QList<QByteArray> rawHeaderList = dl->m_reply->rawHeaderList();
  foreach(QByteArray header, rawHeaderList) {
    QString headerLine = QString(header) + ": " + dl->m_reply->rawHeader(header);
    emit printText(headerLine);
  }

  dl->parseHeader();
  if(dl->error()) {
    qDebug() << dl->error() << endl;
  }

  m_downloads.remove(dl->m_reply);
  dl->m_reply->deleteLater();
  dl->m_reply = 0;

  if(dl->checkRelocation()) {
    dl->relocate();
    downloadRequest(dl);
    return;
  }

  dl->openFile();

  doDownload(dl);
}

void DownloadManager::finished(void)
{
  Download *dl = m_downloads.value(dynamic_cast<QNetworkReply*>(QObject::sender()));

  dl->timerStop();
  dl->m_reply->deleteLater();
  dl->m_reply = 0;

  if(dl->error()) {
    if(dl->errorCnt() < RETRYCNT) {
      qDebug() << dl->error() << endl;
      if(dl->error() == QNetworkReply::RemoteHostClosedError) {
        doDownload(dl);
        return;
      }
    } else {
      dl->closeFile();
      emit failed(dl) ;
      return;
    }
  }

  dl->closeFile();
  qDebug() <<  "Completed, retries:" << dl->errorCnt() << endl;
  emit complete(dl);
}

void DownloadManager::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
  QNetworkReply* reply = dynamic_cast<QNetworkReply*>(QObject::sender());
  Download *dl = m_downloads.value(reply);

  dl->timerStop();
  int percentage = dl->processDownload(bytesReceived, bytesTotal);

  emit downloadProgress(percentage);

  dl->timerStart();
}

void DownloadManager::gotError(QNetworkReply::NetworkError errorcode)
{
  QNetworkReply* reply = dynamic_cast<QNetworkReply*>(QObject::sender());
  qDebug() << __FUNCTION__ << "(" << errorcode << ")";
  reply->deleteLater();
}

void DownloadManager::authenticationRequired(QNetworkReply */*reply*/, QAuthenticator *auth)
{
  // If we try to download from a site with authentication required we will fill the credentials here.
  auth->setUser("");
  auth->setPassword("");
}

void DownloadManager::timeout(QNetworkReply* reply)
{
  qDebug() << __FUNCTION__;
  reply->deleteLater();
}


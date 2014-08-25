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

#ifndef DOWNLOADMANAGER_H_
#define DOWNLOADMANAGER_H_

#include <QObject>
#include <QNetworkReply>

class QFile;
class QNetworkAccessManager;
class QNetworkRequest;
class QTimer;

class DownloadManager : public QObject {
  Q_OBJECT

public:
  explicit DownloadManager(QObject *parent = 0);
  virtual ~DownloadManager(void);

signals:
  void printText(QString qsLine);
  void complete(void);
  void downloadProgress(int percentage);

public slots:
  void download(QUrl url);
  void download(QUrl url, QByteArray *destination);
  void pause(void);
  void resume(void);

private slots:
  void download(void);
  void gotHeader(void);
  void finished(void);
  void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
  void gotError(QNetworkReply::NetworkError errorCode);
  void authenticationRequired(QNetworkReply *reply, QAuthenticator *auth);
  void timeout(void);

private:
  QString m_fileName;
  QFile *m_file;
  QDataStream *m_stream;
  QNetworkAccessManager *m_manager;
  QNetworkRequest *m_request;
  QNetworkReply *m_reply;
  bool m_hostSupportsRanges;
  int m_totalSize;
  int m_downloadSize;
  int m_pausedSize;
  QTimer *m_timer;

  void downloadRequest(QUrl url);

};

#endif // DOWNLOADMANAGER_H_


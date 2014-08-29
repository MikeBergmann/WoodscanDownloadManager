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

#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <QObject>
#include <QNetworkReply>

class QFile;
class QNetworkAccessManager;
class QNetworkRequest;
class QTimer;

class DownloadBase : public QObject {
  Q_OBJECT

  friend class DownloadManager;

public:
  explicit DownloadBase(QObject *parent = 0);
  virtual ~DownloadBase(void);

protected:
  QNetworkRequest *m_request;
  QNetworkReply *m_reply;
};

class Download : public DownloadBase {
  Q_OBJECT

public:
  explicit Download(QUrl &url, QDataStream *stream = 0, QObject *parent = 0);
  virtual ~Download(void);

  void pause(void);
  void fillRequestHeader(void);
  void parseHeader(void);
  void openFile(void);
  QString closeFile(void);
  bool checkRelocation(void);
  void relocate(void);
  void timerStart(void);
  void timerStop(void);
  int processDownload(qint64 bytesReceived, qint64 bytesTotal);
  QNetworkReply::NetworkError error(void);
  int errorCnt(void);
  QString filename(void);

signals:
  void timeout(QNetworkReply *reply);

private slots:
  void timeout(void);

private:
  QString m_fileName;
  QFile *m_file;
  QDataStream *m_stream;

  bool m_hostSupportsRanges;
  int m_totalSize;
  int m_downloadSize;
  int m_pausedSize;
  QNetworkReply::NetworkError m_error;
  int m_errorCnt;

  QString m_newLocation;

  QTimer *m_timer;
};

#endif // DOWNLOAD_H

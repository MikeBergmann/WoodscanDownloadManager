/*
  Copyright 2015 Mike Bergmann, Bones AG

  This file is part of WoodscanDownloadManager.

  The functions within this file are based on work by Kuba Ober, thanks Kuba.
  Check out his profile at StackOverflow: http://stackoverflow.com/users/1329652/kuba-ober

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

#ifndef FILESPLITTER_H
#define FILESPLITTER_H

#include <QMutex>
#include <QBasicTimer>
#include <QFile>

class FileSplitter : public QObject {
  Q_OBJECT

public:
  explicit FileSplitter(QObject *parent = 0);
  ~FileSplitter();

protected:
  QMutex mutable m_mutex;
  QByteArray m_fileBuffer;
  QBasicTimer m_copyTimer, m_progressTimer;
  QString m_error;
  QFile m_inFile, m_outFile;
  qint64 m_sizeTotal, m_sizeDone, m_maxFileSize;
  int m_fileNo;
  int m_shift;

  void close(void);
  void error(QFile& f);
  void finished(void);
  void emitProgress(void);
  void timerEvent(QTimerEvent *ev);
  Q_INVOKABLE void _cancel(void);
  Q_INVOKABLE void _copy(const QString& src, const QString& dst, qint64 maxFileSize = 0);
  QString lastError(void) const;

public slots:
  void copy(const QString& src, const QString& dst, qint64 maxFileSize = 0);
  void cancel(void);

signals:
  void progressed(qint64 done, qint64 total);
  void hasProgressMaximum(int total);
  void hasProgressValue(int done);
  void finished(bool ok, const QString& error);
};

#endif // FILESPLITTER_H

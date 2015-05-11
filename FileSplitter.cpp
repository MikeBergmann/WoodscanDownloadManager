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

#include <QByteArray>
#include <QProgressDialog>
#include <QFileDialog>
#include <QElapsedTimer>
#include <limits>
#include <QDebug>
#include <QTimerEvent>

#include "FileSplitter.h"

FileSplitter::FileSplitter(QObject *parent, bool keepFileClosed)
  : QObject(parent)
  , m_mutex()
  , m_fileBuffer(65536, Qt::Uninitialized) // Copy 64kbytes at a time
  , m_copyTimer()
  , m_progressTimer()
  , m_error()
  , m_inFile()
  , m_outFile()
  , m_sizeTotal(0)
  , m_sizeDone(0)
  , m_maxFileSize(0)
  , m_fileNo(0)
  , m_shift(0)
  , m_keepFileClosed(keepFileClosed)
{
  m_inFile.setObjectName("source");
  m_outFile.setObjectName("destination");
}

FileSplitter::~FileSplitter()
{
}

/**
 * Close files and stop copy.
 */
void FileSplitter::close(void) {
  m_copyTimer.stop();
  m_progressTimer.stop();
  m_inFile.close();
  m_outFile.close();
  m_mutex.unlock();
}

/**
 * Takes the error string from given file and emits an error indication.
 * Closes the files and stops the copy.
 *
 * @param iFile
 */
void FileSplitter::error(QFile& iFile) {
  m_error = iFile.errorString();
  m_error.append(QStringLiteral("(in %1 file").arg(iFile.objectName()));
  emit finished(false, m_error);
  close();
}

void FileSplitter::finished(void) {
  emitProgress();
  emit finished(m_sizeDone == m_sizeTotal, m_error);
  close();
}

void FileSplitter::emitProgress(void) {
  emit progressed(m_sizeDone, m_sizeTotal);
  emit hasProgressValue(m_sizeDone >> m_shift);
}

/**
 * Worker function, cyclically called by QBasicTimer
 *
 * @param event     Timer event
 */
void FileSplitter::timerEvent(QTimerEvent *event) {
  if(event->timerId() == m_copyTimer.timerId()) {
    // Do the copy
    qint64 readSize = m_inFile.read(m_fileBuffer.data(), m_fileBuffer.size());
    if(readSize == -1) {
      error(m_inFile);
      return;
    } else if(readSize == 0) {
      finished();
      return;
    }

    if(m_sizeDone+readSize > m_maxFileSize*m_fileNo) {
      m_outFile.close();
      QFileInfo fi(m_outFile.fileName());

      m_outFile.setFileName(fi.absolutePath() + "/" +
                            QString::number(m_fileNo+1) + "_" +
                            fi.fileName().remove(0,QString::number(m_fileNo).length()+1));
      ++m_fileNo;
      if(!m_outFile.open(QIODevice::WriteOnly)) {
        error(m_outFile);
        return;
      }
    }

    if(m_keepFileClosed && !m_outFile.isOpen()) {
      m_outFile.open(QIODevice::Append);
    }

    qint64 writeSize = m_outFile.write(m_fileBuffer.constData(), readSize);
    if(writeSize == -1) {
      error(m_outFile);
      return;
    }

    if(m_keepFileClosed) {
      m_outFile.close();
    }

    Q_ASSERT(writeSize == readSize);

    m_sizeDone += readSize;
  } else if(event->timerId() == m_progressTimer.timerId()) {
    emitProgress();
  }
}

/**
 * All our stuff lives in the thread. So to cancel the operation from ouside we
 * have to invoke the _cancel method via queued connection.
 */
void FileSplitter::_cancel(void) {
  if(!m_inFile.isOpen())
    return;
  m_error = "Canceled";
  finished();
}


/**
 * Copies a file to another with progress indication.
 * All our stuff (e.g. TBasicTimer) lives in the thread. So to start a copy the
 * operation from ouside we have to invoke the _copy method via queued
 * connection.
 *
 * @param srcPath
 * @param dstPath
 */
void FileSplitter::_copy(const QString& srcPath, const QString& dstPath, qint64 maxFileSize) {

  bool locked = m_mutex.tryLock();
  Q_ASSERT_X(locked, "copy",
             "Another copy is already in progress");
  m_error.clear();

  // Open the files
  m_inFile.setFileName(srcPath);

  if(maxFileSize) {
    QFileInfo fi(dstPath);
    m_outFile.setFileName(fi.absolutePath() + "/" + QString::number(++m_fileNo) + "_" + fi.fileName());
  } else {
    m_outFile.setFileName(dstPath);
  }
  m_maxFileSize = maxFileSize;

  if(!m_inFile.open(QIODevice::ReadOnly)) {
    error(m_inFile);
    return;
  }
  if(!m_outFile.open(QIODevice::WriteOnly)) {
    error(m_outFile);
    return;
  }
  m_sizeTotal = m_inFile.size();
  if(m_sizeTotal < 0)
    return;

  // File size might not fit into an integer, calculate the number of
  // binary digits to shift it right by. Recall that QProgressBar etc.
  // all use int, not qint64!
  m_shift = 0;
  while((m_sizeTotal >> m_shift) >= std::numeric_limits<int>::max())
    m_shift++;

  emit hasProgressMaximum(m_sizeTotal >> m_shift);

  m_sizeDone = 0;
  m_copyTimer.start(0, this);
  m_progressTimer.start(100, this); // Progress is emitted at 10Hz rate
}

/**
 * Get last error.
 * This method is thread safe only when a copy is not in progress.
 *
 * @return Error string
 */
QString FileSplitter::lastError(void) const {
  bool locked = m_mutex.tryLock();
  Q_ASSERT_X(locked, "lastError",
             "A copy is in progress. This method can only be used after copy finished");
  QString error = m_error;
  m_mutex.unlock();
  return error;
}

/**
 * Cancels a pending copy operation. No-op if no copy is underway.
 * This method is thread safe.
 */
void FileSplitter::cancel(void) {
  if(!QMetaObject::invokeMethod(this, "_cancel")) {
    qDebug() << "Invoke Failed";
  }
}

/**
 * Copies a file to another with progress indication.
 * This method is thread safe.
 */
void FileSplitter::copy(const QString& srcPath, const QString& dstPath, qint64 maxFileSize) {
  if(!QMetaObject::invokeMethod(this, "_copy",
                                Q_ARG(const QString&, srcPath),
                                Q_ARG(const QString&, dstPath),
                                Q_ARG(qint64, maxFileSize)
                               )) {
    qDebug() << "Invoke Failed";
  }
}

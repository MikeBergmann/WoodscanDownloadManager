/*
  Copyright 2015 Mike Bergmann, Bones AG

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

#include "FileGroup.h"

#include <QDir>
#include <QDebug>

FileGroup::FileGroup(const QString path, QRegularExpression matcher) :
  m_path(path),
  m_matcher(matcher)
{
}

FileGroup::~FileGroup()
{
}

qint64 FileGroup::size()
{
  qint64 size = 0;
  QDir dir(m_path);
  QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot|QDir::Files);
  for(int i=0; i<list.size();++i) {
    QString name = list.at(i).fileName();
    if(m_matcher.match(name).hasMatch()) {
      qDebug() << m_matcher.match(name).capturedTexts();
      size += list.at(i).size();
    }
  }

  return size;
}

int FileGroup::remove()
{
  int no = 0;
  QDir dir(m_path);
  QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot|QDir::Files);
  for(int i=0; i<list.size();++i) {
    QString name = list.at(i).fileName();
    if(m_matcher.match(name).hasMatch()) {
      qDebug() << m_matcher.match(name).capturedTexts();
      QFile file(list.at(i).filePath());
      if(file.remove()) {
        ++no;
      } else {
        qDebug() << file.errorString() << file.fileName();
      }
    } else {
      qDebug() << name << "not matched";
    }
  }
  return no;
}

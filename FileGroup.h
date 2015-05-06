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

#ifndef FILEGROUP_H
#define FILEGROUP_H

#include <QRegularExpression>

class FileGroup
{
public:
  explicit FileGroup(const QString path, QRegularExpression matcher);
  ~FileGroup();

  qint64 size(void);
  int remove(void);

private:
  QString m_path;
  QRegularExpression m_matcher;
};

#endif // FILEGROUP_H

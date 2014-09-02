/*
  Copyright 2014 Mike Bergmann, Bones AG

  This file is part of WoodscanDownloadManager.

  The functions within this file are based on work by Adam Pierce, thanks Adam.
  You will find his Blog on http://siliconsparrow.com/

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

#ifndef MILESTONE_H_
#define MILESTONE_H_

#include <QString>

bool getMilestoneSerial(unsigned vid, unsigned pid, QString &serial, QString &drives);
qint64 availableDiskSpace(const QString &driveVolume);

#endif



#  Copyright 2014 Mike Bergmann, Bones AG
#
#  This file is part of WoodscanDownloadManager.
#
#  WoodscanDownloadManager is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 2 of the License, or
#  (at your option) any later version.
#
#  WoodscanDownloadManager is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with WoodscanDownloadManager.  If not, see <http://www.gnu.org/licenses/>.

QT       += gui network widgets
TARGET = WoodscanDM
# CONFIG   += console

TEMPLATE = app

LIBS += C:\Qt\Tools\mingw482_32\i686-w64-mingw32\lib\libcfgmgr32.a \
        C:\Qt\Tools\mingw482_32\i686-w64-mingw32\lib\libsetupapi.a \
        C:\Qt\Tools\mingw482_32\i686-w64-mingw32\lib\libuuid.a

DEFINES += _UNICODE

SOURCES += \
    main.cpp \
    mainwidget.cpp \
    DownloadManager/downloadmanager.cpp \
    DownloadManager/download.cpp \
    milestone.cpp \
    FileSplitter.cpp \
    FileGroup.cpp

HEADERS += \
    mainwidget.h \
    DownloadManager/downloadmanager.h \
    DownloadManager/download.h \
    milestone.h \
    FileSplitter.h \
    FileGroup.h

FORMS += \
    form.ui

OTHER_FILES += \
    README.md

TRANSLATIONS = WoodscanDM_de.ts

RC_FILE = WoodscanDM.rc

RESOURCES += \
    WoodscanDM.qrc

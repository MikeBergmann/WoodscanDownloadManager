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

#include <windows.h>

#include <SetupAPI.h>
#include <cfgmgr32.h>
#include <winioctl.h>

#include <initguid.h>
#include <ntddstor.h>
#include <Setupapi.h>
#include <QDebug>
#include <guiddef.h>
#include <tchar.h>

// Namespace
using namespace std;

#ifdef UNICODE
#define QStringToTCHAR(x) (wchar_t*) x.utf16()
#define PQStringToTCHAR(x) (wchar_t*) x->utf16()
#define TCHARToQString(x) QString::fromUtf16((x))
#define TCHARToQStringN(x,y) QString::fromUtf16((x),(y))
#else
#define QStringToTCHAR(x) x.local8Bit().constData()
#define PQStringToTCHAR(x) x->local8Bit().constData()
#define TCHARToQString(x) QString::fromLocal8Bit((x))
#define TCHARToQStringN(x,y) QString::fromLocal8Bit((x),(y))
#endif

/**
 * Finds the device interface for the disk drive with the given interface number.
 *
 * @param DeviceNumber
 *
 * @return DeviceInstance
 */
DEVINST GetDrivesDevInstByDeviceNumber(long DeviceNumber)
{
  const GUID *guid = &GUID_DEVINTERFACE_DISK;

  // Get device interface info handle for all devices attached to system
  HDEVINFO hDevInfo = SetupDiGetClassDevs(guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if(hDevInfo == INVALID_HANDLE_VALUE)
    return 0;

  // Retrieve a context structure for a device interface of a device information set.
  BYTE buf[1024];
  PSP_DEVICE_INTERFACE_DETAIL_DATA pspdidd = (PSP_DEVICE_INTERFACE_DETAIL_DATA)buf;
  SP_DEVICE_INTERFACE_DATA spdid;
  SP_DEVINFO_DATA spdd;
  DWORD dwSize;

  spdid.cbSize = sizeof(spdid);

  // Iterate through all the interfaces and try to match one based on the device number.
  for(DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, NULL, guid, i, &spdid); i++) {
    // Get the device path.
    dwSize = 0;
    SetupDiGetDeviceInterfaceDetail(hDevInfo, &spdid, NULL, 0, &dwSize, NULL);
    if(dwSize == 0 || dwSize > sizeof(buf))
      continue;

    pspdidd->cbSize = sizeof(*pspdidd);
    ZeroMemory((PVOID)&spdd, sizeof(spdd));
    spdd.cbSize = sizeof(spdd);
    if(!SetupDiGetDeviceInterfaceDetail(hDevInfo, &spdid, pspdidd, dwSize, &dwSize, &spdd))
      continue;

    // Open the device.
    HANDLE hDrive = CreateFile(pspdidd->DevicePath, 0,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, 0, NULL);
    if(hDrive == INVALID_HANDLE_VALUE)
      continue;

    // Get the device number.
    STORAGE_DEVICE_NUMBER sdn;
    dwSize = 0;
    if(DeviceIoControl(hDrive, IOCTL_STORAGE_GET_DEVICE_NUMBER,
                       NULL, 0, &sdn, sizeof(sdn), &dwSize, NULL)) {
      // Does it match?
      if(DeviceNumber == (long)sdn.DeviceNumber) {
        CloseHandle(hDrive);
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return spdd.DevInst;
      }
    }
    CloseHandle(hDrive);
  }

  SetupDiDestroyDeviceInfoList(hDevInfo);
  return 0;
}


/**
 * Returns device node if the given device instance belongs to the USB device with the given VID and PID.
 *
 * @param device (in)
 * @param vid    (in)
 * @param pid    (in)
 * @param node   (out)
 *
 * @return true if match, otherwise false
 */
bool matchDevInstToUsbDevice(DEVINST device, DWORD vid, DWORD pid, LPTSTR node)
{
  // This is the string we will be searching for in the device harware IDs.
  TCHAR hwid[64];
  _stprintf(hwid, _T("VID_%04X&PID_%04X"), vid, pid);

  // Get a list of hardware IDs for all USB devices.
  ULONG ulLen;
  CM_Get_Device_ID_List_Size(&ulLen, NULL, CM_GETIDLIST_FILTER_NONE);
  TCHAR *pszBuffer = new TCHAR[ulLen];
  CM_Get_Device_ID_List(NULL, pszBuffer, ulLen, CM_GETIDLIST_FILTER_NONE);

  // Iterate through the list looking for our ID.
  for(LPTSTR pszDeviceID = pszBuffer; *pszDeviceID; pszDeviceID += _tcslen(pszDeviceID) + 1) {
    // Some versions of Windows have the string in upper case and other versions have it
    // in lower case so just make it all upper.
    for(int i = 0; pszDeviceID[i]; i++)
      pszDeviceID[i] = toupper(pszDeviceID[i]);

    if(_tcsstr(pszDeviceID, hwid)) {
      // Found the device, now we want the grandchild device, which is the "generic volume"
      DEVINST MSDInst = 0;
      if(CR_SUCCESS == CM_Locate_DevNode(&MSDInst, pszDeviceID, CM_LOCATE_DEVNODE_NORMAL)) {
        DEVINST DiskDriveInst = 0;
        if(CR_SUCCESS == CM_Get_Child(&DiskDriveInst, MSDInst, 0)) {
          // Now compare the grandchild node against the given device instance.
          do {
            if(device == DiskDriveInst) {
              _tcscpy(node, pszDeviceID);
              return true;
            }
          } while(CR_SUCCESS == CM_Get_Sibling(&DiskDriveInst, DiskDriveInst, 0));
        }
      }
    }
  }
  return false;
}

// Find a USB device by it's Vendor and Product IDs. When found, eject it.
bool getMilestoneSerial(unsigned vid, unsigned pid, QString& serial, QString& drives)
{
  bool found = false;

  TCHAR devicepath[8];
  _tcscpy(devicepath, _T("\\\\.\\?:"));

  TCHAR drivepath[4];
  _tcscpy(drivepath, _T("?:\\"));

  // Iterate through every drive letter and check if it is our device.
  for(TCHAR driveletter = _T('A'); driveletter <= _T('Z'); driveletter++) {
    // We are only interested in CDROM drives.
    drivepath[0] = driveletter;

    if(DRIVE_REMOVABLE != GetDriveType(drivepath))
      continue;

    // Get the "storage device number" for the current drive.
    long DeviceNumber = -1;
    devicepath[4] = driveletter;
    HANDLE hVolume = CreateFile(devicepath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL, OPEN_EXISTING, 0, NULL);
    if(INVALID_HANDLE_VALUE == hVolume)
      continue;

    STORAGE_DEVICE_NUMBER sdn;
    DWORD dwBytesReturned = 0;
    if(DeviceIoControl(hVolume, IOCTL_STORAGE_GET_DEVICE_NUMBER,
                       NULL, 0, &sdn, sizeof(sdn), &dwBytesReturned, NULL))
      DeviceNumber = sdn.DeviceNumber;
    CloseHandle(hVolume);
    if(DeviceNumber < 0)
      continue;

    // Use the data we have collected so far on our drive to find a device instance.
    DEVINST DevInst = GetDrivesDevInstByDeviceNumber(DeviceNumber);

    // If the device instance corresponds to the USB device we are looking for, eject it.
    if(DevInst) {
      TCHAR node[512];
      if(matchDevInstToUsbDevice(DevInst, vid, pid, node)) {
        qDebug() << TCHARToQString((ushort*)node) << endl;
        serial = TCHARToQString((ushort*)node).right(6);
        drives += driveletter;
        found = true;
      }
    }
  }
  return found;
}


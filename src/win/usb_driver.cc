#include "../usb_driver.h"

#include <windows.h>
#include <windowsx.h>
#include <setupapi.h>
#include <devguid.h>
#include <initguid.h>
#include <usbiodef.h>
#include <winioctl.h>
#include <usbioctl.h>
#include <cfgmgr32.h>
#include <assert.h>

#include <map>
#include <bitset>

namespace usb_driver {

static void
print_error(const char *func_name)
{
    DWORD error = GetLastError();
    char *error_str = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
	    | FORMAT_MESSAGE_IGNORE_INSERTS,
	    NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	    (LPTSTR)&error_str, 0, NULL);
    printf("%s failed: error %d: %s", func_name, error, error_str);
    free(error_str);
}

static bool
device_property(HDEVINFO device_info, PSP_DEVINFO_DATA device_info_data,
	DWORD property, std::string &buf_str)
{
    DWORD buf_len = 1000;
    char *buf = (char *)malloc(buf_len);
    assert(buf != NULL);

    bool ok = SetupDiGetDeviceRegistryProperty(device_info, device_info_data,
	    property, NULL, (PBYTE)buf, buf_len, &buf_len);
    if (ok) {
	for (DWORD i = 0; i < buf_len - 1; i++) {
	    if (buf[i] == '\0') {
		buf[i] = '\n';
	    }
	}
	buf_str.append(buf);
    }
    else {
	print_error("SetupDiGetDeviceRegistryProperty()");
    }
    free(buf);
    return ok;
}

static bool
parse_device_id(const char *device_path, std::string &vid,
	std::string &pid, std::string &serial)
{
     const char *p = device_path;
     if (strncmp(p, "USB\\", 4) != 0) {
	return false;
     }
     p += 4;
     if (strncmp(p, "VID_", 4) != 0) {
	return false;
     }
     p += 4;
     vid.clear();
     vid.append("0x");
     while (*p != '&') {
	if (*p == '\0') {
	    return false;
	}
	vid.push_back(tolower(*p));
	p++;
     }
     p++;
     if (strncmp(p, "PID_", 4) != 0) {
	return false;
     }
     p += 4;
     pid.clear();
     pid.append("0x");
     while (*p != '\\') {
	if (*p == '\0') {
	    return false;
	}
	pid.push_back(tolower(*p));
	p++;
     }
     p++;
     serial.clear();
     while (*p != '\0') {
	serial.push_back(toupper(*p));
	p++;
     }
     return true;
}

static std::map<ULONG, unsigned char> device_drives_cache;

static ULONG
device_number_from_handle(HANDLE handle, bool report_error=true)
{
    STORAGE_DEVICE_NUMBER sdn;
    sdn.DeviceNumber = -1;
    DWORD bytes_returned = 0;
    if (DeviceIoControl(handle, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn,
		sizeof(sdn), &bytes_returned, NULL)) {
	return sdn.DeviceNumber;
    }
    if (report_error) {
	print_error("DeviceIOControl(IOCTL_STORAGE_GET_DEVICE_NUMBER)");
    }
    return 0;
}

static std::string
drive_for_device_number(ULONG device_number)
{
    if (device_drives_cache.size() == 0) {
	std::bitset<32> drives(GetLogicalDrives());
	for (char c = 'D'; c <= 'Z'; c++) {
	    if (!drives[c - 'A']) {
		continue;
	    }
	    std::string path = std::string("\\\\.\\") + c + ":";

	    HANDLE drive_handle = CreateFileA(path.c_str(), GENERIC_READ,
		    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		    OPEN_EXISTING, FILE_FLAG_NO_BUFFERING
		    | FILE_FLAG_RANDOM_ACCESS, NULL);
	    if (drive_handle == INVALID_HANDLE_VALUE) {
		print_error("CreateFileA()");
		continue;
	    }

	    ULONG num = device_number_from_handle(drive_handle, false);
	    if (num != 0) {
		device_drives_cache[num] = c;
	    }

	    CloseHandle(drive_handle);
	}
    }

    std::map<ULONG, unsigned char>::iterator iter =
	device_drives_cache.find(device_number);
    if (iter == device_drives_cache.end()) {
	return "";
    }
    std::string mount = "";
    mount.push_back(iter->second);
    mount.append(":");
    return mount;
}

std::vector<struct USBDrive *>
GetDevices(void)
{
    std::vector<struct USBDrive *> devices;

    const GUID *guid = &GUID_DEVINTERFACE_DISK;
    HDEVINFO device_info = SetupDiGetClassDevs(guid, NULL, NULL,
	    (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    if (device_info != INVALID_HANDLE_VALUE) {
	int index = 0;
	while (true) {
	    SP_DEVINFO_DATA device_info_data;
	    device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
	    if (!SetupDiEnumDeviceInfo(device_info, index,
			&device_info_data)) {
		if (GetLastError() != ERROR_NO_MORE_ITEMS) {
		    print_error("SetupDiEnumDeviceInfo()");
		}
		break;
	    }
	    index++;

	    std::string device_name;
	    if (!device_property(device_info, &device_info_data,
			SPDRP_FRIENDLYNAME, device_name)) {
		continue;
	    }

	    SP_DEVICE_INTERFACE_DATA interface_data;
	    interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	    if (!SetupDiEnumDeviceInterfaces(device_info, 0, guid, index - 1,
			&interface_data)) {
		print_error("SetupDiEnumDeviceInterfaces()");
		continue;
	    }

	    ULONG interface_detail_len = 1000;
	    SP_DEVICE_INTERFACE_DETAIL_DATA *interface_detail =
		(SP_DEVICE_INTERFACE_DETAIL_DATA *)malloc(interface_detail_len);
	    assert(interface_detail != NULL);
	    interface_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
	    SP_DEVINFO_DATA devinfo_data;
	    devinfo_data.cbSize = sizeof(SP_DEVINFO_DATA);
	    if (!SetupDiGetDeviceInterfaceDetail(device_info,
			&interface_data, interface_detail,
			interface_detail_len, &interface_detail_len,
			&devinfo_data)) {
		print_error("SetupDiGetDeviceInterfaceDetail()");
		continue;
	    }

            HANDLE handle = CreateFile(interface_detail->DevicePath,
		    0, FILE_SHARE_READ | FILE_SHARE_WRITE,
		    NULL, OPEN_EXISTING, 0, NULL);
	    if (handle == INVALID_HANDLE_VALUE) {
		print_error("CreateFile()");
		continue;
	    }

	    std::string mount = "";
	    ULONG device_number = device_number_from_handle(handle);
	    if (device_number != 0) {
		mount = drive_for_device_number(device_number);
	    }

	    CloseHandle(handle);

	    DEVINST dev_inst_parent;
	    if (CM_Get_Parent(&dev_inst_parent, devinfo_data.DevInst, 0)
		    != CR_SUCCESS) {
		continue;
	    }

	    char dev_inst_parent_id[MAX_DEVICE_ID_LEN];
	    if (CM_Get_Device_ID(dev_inst_parent, dev_inst_parent_id,
			MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) {
		continue;
	    }

	    std::string vid, pid, serial;
	    if (!parse_device_id(dev_inst_parent_id, vid, pid,
			serial)) {
		continue;
	    }

	    struct USBDrive *usb_info = new struct USBDrive;
	    assert(usb_info != NULL);

	    usb_info->uid = "";
	    usb_info->uid.append(vid);
	    usb_info->uid.append("-");
	    usb_info->uid.append(pid);
	    usb_info->uid.append("-");
	    usb_info->uid.append(serial);

	    usb_info->location_id = ""; // TODO
	    usb_info->product_id = pid;
	    usb_info->vendor_id = vid;
	    usb_info->product_str = device_name;
	    usb_info->serial_str = serial;
	    usb_info->vendor_str = ""; // TODO
	    usb_info->mount = mount;

	    devices.push_back(usb_info);
	}
    }
    return devices;
}

struct USBDrive *
GetDevice(const std::string &device_id)
{
    // TODO: implement
    return NULL;
}

bool
Unmount(const std::string &device_id)
{
    // TODO: implement
    return false;
}

void
RegisterWatcher(USBWatcher *watcher)
{
    // TODO: implement
}

void WaitForEvents(void)
{
    // TODO: implement
}

}  // namespace usb_driver

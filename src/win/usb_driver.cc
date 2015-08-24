#include "../usb_driver.h"

#include <windows.h>
#include <windowsx.h>
#include <setupapi.h>
#include <devguid.h>
#include <initguid.h>
#include <usbiodef.h>
#include <assert.h>

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
    DWORD buf_len = 100;
    char *buf = (char *)malloc(buf_len);
    assert(*buf != NULL);

    bool ok = SetupDiGetDeviceRegistryProperty(device_info, device_info_data,
	    property, NULL, (PBYTE)buf, buf_len, NULL);
    if (ok) {
	buf_str.append(buf);
    }
    else {
	print_error("SetupDiGetDeviceRegistryProperty()");
    }
    free(buf);
    return ok;
}

static bool
parse_device_path(const char *device_path, std::string &vid,
	std::string &pid, std::string &serial)
{
     const char *p = device_path;
     if (strncmp(p, "\\\\?\\usb#", 8) != 0) {
	return false;
     }
     p += 8;
     if (strncmp(p, "vid_", 4) != 0) {
	return false;
     }
     p += 4;
     vid.clear();
     vid.append("0x");
     while (*p != '&') {
	if (*p == '\0') {
	    return false;
	}
	vid.push_back(*p);
	p++;
     }
     p++;
     if (strncmp(p, "pid_", 4) != 0) {
	return false;
     }
     p += 4;
     pid.clear();
     pid.append("0x");
     while (*p != '#') {
	if (*p == '\0') {
	    return false;
	}
	pid.push_back(*p);
	p++;
     }
     p++;
     serial.clear();
     while (*p != '#') {
	if (*p == '\0') {
	    return false;
	}
	serial.push_back(toupper(*p));
	p++;
     }
     return true;
}

std::vector<struct USBDrive *>
GetDevices(void)
{
    std::vector<struct USBDrive *> devices;

    const GUID *guid = &GUID_DEVINTERFACE_USB_DEVICE;
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

	    std::string device_desc;
	    if (!device_property(device_info, &device_info_data,
			SPDRP_DEVICEDESC, device_desc)) {
		continue;
	    }

	    SP_DEVICE_INTERFACE_DATA interface_data;
	    interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	    if (!SetupDiEnumDeviceInterfaces(device_info, 0, guid, index - 1,
			&interface_data)) {
		print_error("SetupDiEnumDeviceInterfaces()");
		continue;
	    }

	    ULONG interface_detail_len = 100;
	    SP_DEVICE_INTERFACE_DETAIL_DATA *interface_detail =
		(SP_DEVICE_INTERFACE_DETAIL_DATA *)malloc(interface_detail_len);
	    assert(interface_detail != NULL);
	    interface_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
	    if (!SetupDiGetDeviceInterfaceDetail(device_info,
			&interface_data, interface_detail,
			interface_detail_len,
			&interface_detail_len, NULL)) {
		print_error("SetupDiGetDeviceInterfaceDetail()");
		continue;
	    }

	    std::string vid, pid, serial;
	    if (!parse_device_path(interface_detail->DevicePath, vid, pid,
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
	    usb_info->product_str = device_desc;
	    usb_info->serial_str = serial;
	    usb_info->vendor_str = ""; // TODO
	    usb_info->mount = ""; // TODO

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

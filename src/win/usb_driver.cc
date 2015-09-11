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
#include <dbt.h>
#include <assert.h>

#include <map>
#include <bitset>

namespace usb_driver {

static std::vector<struct USBDrive *> all_devices;

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
		    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		    FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS, NULL);
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

struct USBDrive_Win {
    DEVINST device_inst;
};

std::vector<struct USBDrive *>
GetDevices(void)
{
    for (unsigned int i = 0; i < all_devices.size(); i++) {
	struct USBDrive *usb_info = all_devices[i];
	struct USBDrive_Win *usb_info_win =
	    (struct USBDrive_Win *)usb_info->opaque;
	delete usb_info_win;
	delete usb_info;
    }
    all_devices.clear();

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

	    free(interface_detail);

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
	    struct USBDrive_Win *usb_info2 = new struct USBDrive_Win;
	    assert(usb_info2 != NULL);

	    usb_info->opaque = (void *)usb_info2;
	    usb_info2->device_inst = dev_inst_parent;

	    usb_info->uid = "";
	    usb_info->uid.append(vid);
	    usb_info->uid.append("-");
	    usb_info->uid.append(pid);
	    usb_info->uid.append("-");
	    usb_info->uid.append(serial);

	    usb_info->location_id = ""; // Not set.
	    usb_info->product_id = pid;
	    usb_info->vendor_id = vid;
	    usb_info->product_str = device_name;
	    usb_info->serial_str = serial;
	    usb_info->vendor_str = ""; // TODO
	    usb_info->mount = mount;

	    all_devices.push_back(usb_info);
	}
    }
    return all_devices;
}

struct USBDrive *
GetDevice(const std::string &device_id)
{
    for (unsigned int i = 0; i < all_devices.size(); i++) {
	struct USBDrive *usb_info = all_devices[i];
	if (usb_info->uid == device_id) {
	    return usb_info;
	}
    }
    return NULL;
}

bool
Unmount(const std::string &device_id)
{
    struct USBDrive *usb_info = GetDevice(device_id);
    if (usb_info == NULL || usb_info->mount.size() == 0) {
	return false;
    }

    PNP_VETO_TYPE VetoType = PNP_VetoTypeUnknown;
    char VetoName[MAX_PATH];
    VetoName[0] = '\0';

    if (CM_Request_Device_Eject(
		((struct USBDrive_Win *)usb_info->opaque)->device_inst,
		&VetoType, VetoName, MAX_PATH, 0) == CR_SUCCESS) {
	return true;
    }
    printf("error when requesting device eject %d: %s\n", VetoType, VetoName);
    return false;
}

static USBWatcher *watcher = NULL;

static char
drive_letter_from_unitmask(DWORD unitmask)
{
    char i;
    for (i = 0; i < 26; ++i) {
	if (unitmask & 0x1) {
	    break;
	}
	unitmask = unitmask >> 1;
    }
    return i + 'A';
}

static LRESULT CALLBACK
WndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    if (uiMsg == WM_DEVICECHANGE
	    && (wParam == DBT_DEVICEARRIVAL
		|| wParam == DBT_DEVICEREMOVECOMPLETE)) {
	DEV_BROADCAST_HDR *device = (DEV_BROADCAST_HDR *)lParam;
	if (device->dbch_devicetype == DBT_DEVTYP_VOLUME) {
	    DEV_BROADCAST_VOLUME *volume = (DEV_BROADCAST_VOLUME *)lParam;
	    char drive = drive_letter_from_unitmask(volume->dbcv_unitmask);
	    if (wParam == DBT_DEVICEARRIVAL) {
		GetDevices();
	    }
	    for (int i = 0; i < all_devices.size(); i++) {
		struct USBDrive *usb_info = all_devices[i];
		if (usb_info->mount[0] == drive) {
		    if (wParam == DBT_DEVICEARRIVAL) {
			watcher->mount(usb_info);
		    }
		    else if (wParam == DBT_DEVICEREMOVECOMPLETE) {
			watcher->unmount(usb_info);
		    }
		    return 0;
		}
	    }
	}
    }
    return DefWindowProc(hWnd, uiMsg, wParam, lParam);
}

void
RegisterWatcher(USBWatcher *_watcher)
{
    if (watcher != _watcher) {
	if (watcher != NULL) {
	    delete watcher;
	}
	watcher = _watcher;
    }

    static bool init = false;
    if (init) {
	return;
    }
    init = true;

    HINSTANCE hInstance = reinterpret_cast<HINSTANCE>(GetModuleHandle(NULL));

    WNDCLASS wndClass = {0};
    wndClass.lpfnWndProc = &WndProc;
    wndClass.lpszClassName = TEXT("usb-driver");
    wndClass.hInstance = hInstance;

    if (!RegisterClass(&wndClass)) {
	print_error("RegisterClass");
	return;
    }

    HWND lua = CreateWindow(wndClass.lpszClassName, NULL, 0, CW_USEDEFAULT,
	    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL,
	    hInstance, NULL);
    if (lua == NULL) {
	print_error("CreateWindow");
	return;
    }

    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
    ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));

    NotificationFilter.dbcc_size = sizeof(NotificationFilter);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    NotificationFilter.dbcc_reserved = 0;
    NotificationFilter.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

    if (RegisterDeviceNotification(lua, &NotificationFilter,
		DEVICE_NOTIFY_WINDOW_HANDLE) == NULL) {
	print_error("RegisterDeviceNotification");
	return;
    }
}

void
WaitForEvents(void)
{
    printf("WaitForEvents() is not implemented on Windows.\n");
}

}  // namespace usb_driver

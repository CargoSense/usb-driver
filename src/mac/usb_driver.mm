#include "../usb_driver.h"

#import <Foundation/Foundation.h>
#import <IOKit/IOBSD.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/IOCFPlugIn.h>
#import <IOKit/usb/IOUSBLib.h>
#import <IOKit/storage/IOStorageDeviceCharacteristics.h>
#import <DiskArbitration/DiskArbitration.h>

#include <map>

namespace usb_driver {

static std::vector<struct USBDrive *> all_devices;
static std::map<std::string, struct USBDrive *> all_devices_map;

bool
Unmount(const std::string &volume)
{
    if (volume.size() > 0) {
	DASessionRef da_session = DASessionCreate(kCFAllocatorDefault);
	assert(da_session != NULL);
	CFURLRef volume_path = CFURLCreateFromFileSystemRepresentation(
		kCFAllocatorDefault, (const UInt8 *)volume.c_str(),
		volume.size(), true);
	assert(volume_path != NULL);
	DADiskRef disk = DADiskCreateFromVolumePath(kCFAllocatorDefault,
		da_session, volume_path);
	if (disk != NULL) {
	    // TODO: pass error callback and escalate error to JS.
	    DADiskUnmount(disk, kDADiskUnmountOptionDefault, NULL, NULL);
	    CFRelease(disk);
	    return true;
	}
	CFRelease(volume_path);
	CFRelease(da_session);
    }
    return false;
}

struct USBDrive *
GetDevice(const std::string &identifier)
{
    std::map<std::string, struct USBDrive *>::iterator iter =
	all_devices_map.find(identifier);
    if (iter == all_devices_map.end()) {
	return NULL;
    }
    return iter->second;
}

struct USBDrive_Mac {
    std::string bsd_disk_name;
};

static struct USBDrive *
usb_service_object(io_service_t usb_service)
{
    CFMutableDictionaryRef properties = NULL;
    kern_return_t err = IORegistryEntryCreateCFProperties(usb_service,
	    &properties, kCFAllocatorDefault, kNilOptions);
    if (err != kIOReturnSuccess) {
	NSLog(@"IORegistryEntryCreateCFProperties() failed: %s",
		mach_error_string(err));
	return NULL;
    }

    struct USBDrive *usb_info = new struct USBDrive;
    assert(usb_info != NULL);
    struct USBDrive_Mac *usb_info_mac = new struct USBDrive_Mac;
    assert(usb_info_mac != NULL);
    usb_info->opaque = (void *)usb_info_mac;

#define USB_INFO_ATTR(dict, key, var) \
    do { \
	id _obj = (id)CFDictionaryGetValue(dict, CFSTR(key)); \
	if (_obj != nil) { \
	    var = [[_obj description] UTF8String]; \
	} \
    } \
    while (0)

    USB_INFO_ATTR(properties, "locationID", usb_info->location_id);
    USB_INFO_ATTR(properties, "idProduct", usb_info->product_id);
    USB_INFO_ATTR(properties, "idVendor", usb_info->vendor_id);
    USB_INFO_ATTR(properties, "USB Product Name", usb_info->product_str);
    USB_INFO_ATTR(properties, "USB Serial Number", usb_info->serial_str);
    USB_INFO_ATTR(properties, "USB Vendor Name", usb_info->vendor_str);
    USB_INFO_ATTR(properties, "USB Address", usb_info->device_address);

#undef USB_INFO_ATTR
    CFRelease(properties);

    id bsd_name = (id)IORegistryEntrySearchCFProperty(usb_service,
	    kIOServicePlane, CFSTR(kIOBSDNameKey), kCFAllocatorDefault,
	    kIORegistryIterateRecursively);
    if (bsd_name != nil) {
	usb_info_mac->bsd_disk_name = [(id)bsd_name UTF8String];
	usb_info_mac->bsd_disk_name.append("s1");
	std::string disk_path = "/dev/";
	disk_path.append(usb_info_mac->bsd_disk_name);

	DASessionRef da_session = DASessionCreate(kCFAllocatorDefault);
	assert(da_session != NULL);
	DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault,
		da_session, disk_path.c_str());
	if (disk != NULL) {
	    CFDictionaryRef desc = DADiskCopyDescription(disk);
	    if (desc != NULL) {
		id path = (id)CFDictionaryGetValue(desc,
                        kDADiskDescriptionVolumePathKey);
		if (path != nil) {
		    std::string path_str = [[path description] UTF8String];
		    // Since we get an URI, we need to determine the UNIX
		    // path from it.
		    size_t off = path_str.find("/Volumes");
		    if (off != std::string::npos) {
			usb_info->mount = path_str.substr(off);
		    }
		}
		CFRelease(desc);
	    }
	    CFRelease(disk);
	}
	CFRelease(da_session);
    }

    return usb_info;
}

std::vector<struct USBDrive *>
GetDevices(void)
{
    mach_port_t master_port;
    kern_return_t err = IOMasterPort(MACH_PORT_NULL, &master_port);
    if (err != kIOReturnSuccess) {
	NSLog(@"IOMasterPort() failed: %s",
		mach_error_string(err));
    }

    CFDictionaryRef usb_matching = IOServiceMatching(kIOUSBDeviceClassName);
    assert(usb_matching != NULL);

    io_iterator_t iter = 0;
    err = IOServiceGetMatchingServices(kIOMasterPortDefault, usb_matching,
	    &iter);
    if (err != kIOReturnSuccess) {
	NSLog(@"IOServiceGetMatchingServices() failed: %s",
		mach_error_string(err));
    }
    else {
	all_devices.clear();

	io_service_t usb_service;
	while ((usb_service = IOIteratorNext(iter)) != 0) {
	    struct USBDrive *usb_info = usb_service_object(usb_service);
	    if (usb_info != NULL) {
		all_devices_map[usb_info->location_id] = usb_info;
		all_devices.push_back(usb_info);
	    }
	    IOObjectRelease(usb_service);
	}
    }
    mach_port_deallocate(mach_task_self(), master_port);
    return all_devices;
}

static USBWatcher *watcher = NULL;

static struct USBDrive *
usb_info_from_DADisk(DADiskRef disk, bool force_lookup)
{
    const char *bsd_disk_name = DADiskGetBSDName(disk);
    assert(bsd_disk_name != NULL);

    if (force_lookup) {
	GetDevices();
    }

    for (unsigned int i = 0; i < all_devices.size(); i++) {
	struct USBDrive *usb_info = all_devices[i];
	if (((struct USBDrive_Mac *)usb_info->opaque)->bsd_disk_name
		== bsd_disk_name) {
	    return usb_info;
	}
    }
    return NULL;
}

static void
watcher_disk_appeared(DADiskRef disk, void *context)
{
    watcher->attached(usb_info_from_DADisk(disk, true));
}

static void
watcher_disk_disappeared(DADiskRef disk, void *context)
{
    watcher->detached(usb_info_from_DADisk(disk, false));
}

static DADissenterRef
watcher_disk_mount(DADiskRef disk, void *context)
{
    watcher->mount(usb_info_from_DADisk(disk, true));
    return NULL; // approve
}

static DADissenterRef
watcher_disk_unmount(DADiskRef disk, void *context)
{
    watcher->unmount(usb_info_from_DADisk(disk, false));
    return NULL; // approve
}

void
RegisterWatcher(USBWatcher *_watcher)
{
    if (watcher != NULL) {
	NSLog(@"RegisterWatcher(): watcher already registered");
	return;
    }
    watcher = _watcher;

    DASessionRef da_session = DASessionCreate(kCFAllocatorDefault);
    assert(da_session != NULL);

    CFMutableDictionaryRef match_dict = CFDictionaryCreateMutable(
	    kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
    CFStringRef usb_prot_key = CFStringCreateWithCString(kCFAllocatorDefault,
	    kIOPropertyPhysicalInterconnectTypeUSB, kCFStringEncodingASCII);
    CFDictionaryAddValue(match_dict, kDADiskDescriptionDeviceProtocolKey,
	    usb_prot_key);
    CFRelease(usb_prot_key);

    DARegisterDiskAppearedCallback(da_session, match_dict,
	    watcher_disk_appeared, NULL);
    DARegisterDiskDisappearedCallback(da_session, match_dict,
	    watcher_disk_disappeared, NULL);
    DARegisterDiskMountApprovalCallback(da_session, match_dict,
	    watcher_disk_mount, NULL);
    DARegisterDiskUnmountApprovalCallback(da_session, match_dict,
	    watcher_disk_unmount, NULL);

    CFRelease(match_dict);

    DASessionScheduleWithRunLoop(da_session, CFRunLoopGetCurrent(),
	    kCFRunLoopDefaultMode);
}

void
WaitForEvents(void)
{
    CFRunLoopRun();
}

}  // namespace usb_driver

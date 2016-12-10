#include "../usb_driver.h"

#import <Foundation/Foundation.h>
#import <IOKit/IOBSD.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/IOCFPlugIn.h>
#import <IOKit/usb/IOUSBLib.h>
#import <IOKit/storage/IOStorageDeviceCharacteristics.h>
#import <DiskArbitration/DiskArbitration.h>
#import <mach/mach.h>

#include <map>

static usb_driver::USBWatcher *watcher = NULL;

@interface WatcherDelayedCallback : NSObject
{
    @public
	struct usb_driver::USBDrive *usb_info;
}
@end

@implementation WatcherDelayedCallback

- (void)delay_mount
{
    [self performSelector:@selector(send_mount_event) withObject:nil
	afterDelay:0.1];
}

- (void)send_mount_event
{
    if (usb_info != NULL) {
	usb_driver::GetDevices();
	if (usb_info->mount.size() > 0) {
	    watcher->mount(usb_info);
	    usb_info = NULL;
	    [self autorelease];
	}
	else {
	    [self delay_mount];
	}
    }
}

@end

static bool
el_capitan_or_higher(void)
{
    NSOperatingSystemVersion osx_version =
	[[NSProcessInfo processInfo] operatingSystemVersion];
    return osx_version.majorVersion == 10 && osx_version.minorVersion >= 11;
}

namespace usb_driver {

static std::vector<struct USBDrive *> all_devices;
unsigned long all_unserialized_devices_counter = 0;

bool
Unmount(const std::string &identifier)
{
    struct USBDrive *usb_info = GetDevice(identifier);
    if (usb_info != NULL && usb_info->mount.size() > 0) {
	DASessionRef da_session = DASessionCreate(kCFAllocatorDefault);
	assert(da_session != NULL);
	CFURLRef volume_path = CFURLCreateFromFileSystemRepresentation(
		kCFAllocatorDefault, (const UInt8 *)usb_info->mount.c_str(),
		usb_info->mount.size(), true);
	assert(volume_path != NULL);
	DADiskRef disk = DADiskCreateFromVolumePath(kCFAllocatorDefault,
		da_session, volume_path);
	if (disk != NULL) {
	    // TODO: pass error callback and escalate error to JS.
	    DADiskUnmount(disk, kDADiskUnmountOptionDefault, NULL, NULL);
	    CFRelease(disk);
	    usb_info->mount = "";
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
    for (unsigned int i = 0; i < all_devices.size(); i++) {
	if (all_devices[i]->uid == identifier) {
	    return all_devices[i];
	}
    }
    return NULL;
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

#define PROP_VAL(dict, key) \
    ({ \
	id _obj = (id)CFDictionaryGetValue(dict, CFSTR(key)); \
	_obj != nil ? [[_obj description] UTF8String] : ""; \
    })

    struct USBDrive *usb_info = NULL;
    struct USBDrive_Mac *usb_info_mac = NULL;
    std::string location_id = PROP_VAL(properties, "locationID");

    for (unsigned int i = 0; i < all_devices.size(); i++) {
	if (all_devices[i]->location_id == location_id) {
	    usb_info = all_devices[i];
	    usb_info_mac = (struct USBDrive_Mac *)usb_info->opaque;
	    break;
	}
    }

    if (usb_info == NULL) {
	usb_info = new struct USBDrive;
	assert(usb_info != NULL);
	usb_info_mac = new struct USBDrive_Mac;
	assert(usb_info_mac != NULL);
	usb_info->opaque = (void *)usb_info_mac;
	all_devices.push_back(usb_info);
    }

    usb_info->location_id = location_id;
    usb_info->vendor_id = PROP_VAL(properties, "idVendor");
    usb_info->product_id = PROP_VAL(properties, "idProduct");
    usb_info->serial_str = PROP_VAL(properties, "USB Serial Number");
    usb_info->product_str = PROP_VAL(properties, "USB Product Name");
    usb_info->vendor_str = PROP_VAL(properties, "USB Vendor Name");

#define HEXIFY(str) \
    do { \
	char _tmp[100]; \
	snprintf(_tmp, sizeof _tmp, "0x%lx", atol(str.c_str())); \
	str = _tmp; \
    } \
    while (0)

    HEXIFY(usb_info->product_id);
    HEXIFY(usb_info->vendor_id);

#undef HEXIFY
#undef PROP_VAL

    CFRelease(properties);

    // Generate unique device ID (https://github.com/CargoSense/usb-driver/wiki/Device-Object).
    if (usb_info->uid.size() == 0) {
	usb_info->uid.append(usb_info->vendor_id);
	usb_info->uid.append("-");
	usb_info->uid.append(usb_info->product_id);
	usb_info->uid.append("-");
	if (usb_info->serial_str.size() > 0) {
	    usb_info->uid.append(usb_info->serial_str);
	}
	else {
	    char buf[100];
	    snprintf(buf, sizeof buf, "0x%lx",
		    all_unserialized_devices_counter++);
	    usb_info->uid.append(buf);
	}
    }

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
		id path_url = (id)CFDictionaryGetValue(desc,
                        kDADiskDescriptionVolumePathKey);
		if (path_url != nil
			&& [path_url isKindOfClass:[NSURL class]]) {
		    usb_info->mount = [path_url fileSystemRepresentation];
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

    CFDictionaryRef usb_matching =
	IOServiceMatching(el_capitan_or_higher()
		? "IOUSBHostDevice" : kIOUSBDeviceClassName);
    assert(usb_matching != NULL);

    std::vector<struct USBDrive *> devices;
    io_iterator_t iter = 0;
    err = IOServiceGetMatchingServices(kIOMasterPortDefault, usb_matching,
	    &iter);
    if (err != kIOReturnSuccess) {
	NSLog(@"IOServiceGetMatchingServices() failed: %s",
		mach_error_string(err));
    }
    else {
	io_service_t usb_service;
	while ((usb_service = IOIteratorNext(iter)) != 0) {
	    struct USBDrive *usb_info = usb_service_object(usb_service);
	    if (usb_info != NULL) {
		devices.push_back(usb_info);
	    }
	    IOObjectRelease(usb_service);
	}
    }
    mach_port_deallocate(mach_task_self(), master_port);
    return devices;
}

static struct USBDrive *
usb_info_from_DADisk(DADiskRef disk, bool force_lookup, bool remove_after)
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
	    if (remove_after) {
		all_devices.erase(all_devices.begin() + i);
	    }
	    return usb_info;
	}
    }
    return NULL;
}

static void
watcher_disk_appeared(DADiskRef disk, void *context)
{
    struct USBDrive *usb_info = usb_info_from_DADisk(disk, true, false);
    if (usb_info != NULL) {
	if (el_capitan_or_higher()) {
	    // Work around what seems to be a race condition in 10.11 where
	    // the DAVolumePath key isn't set yet.
	    WatcherDelayedCallback *o = [[WatcherDelayedCallback alloc] init];
	    o->usb_info = usb_info;
	    [o delay_mount];
	}
	else {
	    watcher->mount(usb_info);
	}
    }
}

static void
watcher_disk_disappeared(DADiskRef disk, void *context)
{
    struct USBDrive *usb_info = usb_info_from_DADisk(disk, false, true);
    if (usb_info != NULL) {
	watcher->detached(usb_info);
    }
}

#if 0
static DADissenterRef
watcher_disk_mount(DADiskRef disk, void *context)
{
    watcher->mount(usb_info_from_DADisk(disk, true));
    return NULL; // approve
}
#endif

static DADissenterRef
watcher_disk_unmount(DADiskRef disk, void *context)
{
    struct USBDrive *usb_info = usb_info_from_DADisk(disk, false, false);
    if (usb_info != NULL) {
	watcher->unmount(usb_info);
    }
    return NULL; // approve
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
//    DARegisterDiskMountApprovalCallback(da_session, match_dict,
//	    watcher_disk_mount, NULL);
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

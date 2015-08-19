#include "../usb_driver.h"

#import <Foundation/Foundation.h>
#import <IOKit/IOBSD.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/IOCFPlugIn.h>
#import <IOKit/usb/IOUSBLib.h>
#import <DiskArbitration/DiskArbitration.h>

#include <map>

namespace usb_driver {

static std::map<std::string, struct USBDrive *> all_devices;

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
	all_devices.find(identifier);
    if (iter == all_devices.end()) {
	return NULL;
    }
    return iter->second;
}

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
	    kIOServicePlane, CFSTR (kIOBSDNameKey), kCFAllocatorDefault,
	    kIORegistryIterateRecursively);
    if (bsd_name != nil) {
	std::string usb_device = "/dev/";
	usb_device.append([(id)bsd_name UTF8String]);
	usb_device.append("s1");

	DASessionRef da_session = DASessionCreate(kCFAllocatorDefault);
	assert(da_session != NULL);
	DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault,
		da_session, usb_device.c_str());
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
		all_devices[usb_info->location_id] = usb_info;
		devices.push_back(usb_info);
	    }
	    IOObjectRelease(usb_service);
	}
    }
    mach_port_deallocate(mach_task_self(), master_port);
    return devices;
}

}  // namespace usb_driver

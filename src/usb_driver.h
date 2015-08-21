#ifndef SRC_USB_DRIVER_H_
#define SRC_USB_DRIVER_H_

#include <string>
#include <vector>

namespace usb_driver {
    struct USBDrive {
	std::string location_id;
	std::string product_id;
	std::string vendor_id;
	std::string product_str;
	std::string serial_str;
	std::string vendor_str;
	std::string device_address;
	std::string mount;
	void *opaque;
    };

    class USBWatcher {
	public:
	virtual ~USBWatcher() { }
	virtual void attached(struct USBDrive *usb_info) = 0;
	virtual void detached(struct USBDrive *usb_info) = 0;
	virtual void mount(struct USBDrive *usb_info) = 0;
	virtual void unmount(struct USBDrive *usb_info) = 0;
    };

    std::vector<struct USBDrive *> GetDevices();
    struct USBDrive *GetDevice(const std::string &id);
    bool Unmount(const std::string &volume);
    void RegisterWatcher(USBWatcher *watcher);
    void WaitForEvents(void);
}

#endif  // SRC_USB_DRIVER_H_

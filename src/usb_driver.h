#ifndef SRC_USB_DRIVER_H_
#define SRC_USB_DRIVER_H_

#include <string>
#include <vector>

namespace usb_driver {
  struct USBDrive {
    std::string uid;
    std::string location_id;
    std::string product_id;
    std::string vendor_id;
    std::string product_str;
    std::string serial_str;
    std::string vendor_str;
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

  std::vector<struct USBDrive *> GetDevices(void);
  struct USBDrive *GetDevice(const std::string &device_id);
  bool Unmount(const std::string &device_id);
  void RegisterWatcher(USBWatcher *watcher);
  void WaitForEvents(void);
}

#endif  // SRC_USB_DRIVER_H_

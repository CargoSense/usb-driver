#ifndef SRC_USB_DRIVER_H_
#define SRC_USB_DRIVER_H_

#include <string>

namespace usb_driver {
  
  void GetDevices();
  
  void GetDevice(const std::string& id);

  bool Unmount(const std::string& volume);

}  // namespace usb_driver

#endif  // SRC_USB_DRIVER_H_

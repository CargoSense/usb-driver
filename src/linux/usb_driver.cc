#include "../usb_driver.h"

#include <iostream>

namespace usb_driver {

bool Unmount(const std::string& volume) {
  std::cout << "Unmount volume: ";
  std::cout << volume << std::endl;
  if (umount2(volume.c_str()) == 0) {
    return true;
  } else {
    return false;
  }
}

void GetDevice(const std::string& identifier) {
  std::cout << "Get Device: ";
  std::cout << identifier << std::endl;
}

void GetDevices() {
  std::cout << "Get Devices";
  std::cout << std::endl;
}

}  // namespace usb_driver

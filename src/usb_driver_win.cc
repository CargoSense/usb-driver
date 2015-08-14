#include "usb_driver.h"

#include <iostream>

namespace usb_driver {

bool Unmount(const std::string& volume) {
  std::cout << "Unmount volume: ";
  std::cout << volume  << std::endl;
  if(DeleteVolumeMountPoint(volume.c_str())) {
    return true;
  } else {
    return false;
  }
}

}  // namespace usb_driver

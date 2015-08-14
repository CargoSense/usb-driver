#include "usb_driver.h"

#include <iostream>

namespace usb_driver {

void Unmount(const std::string& volume) {
  std::cout << "Unmount volume: ";
  std::cout << volume << std::endl;
}

}  // namespace usb_driver

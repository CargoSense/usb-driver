#include "usb_driver.h"

#import <Foundation/Foundation.h>

namespace usb_driver {

bool Unmount(const std::string& volume) {
  NSLog(@"USB DRIVER: UNMOUNT VOLUME: %s", volume.c_str());
  return false;
}

}  // namespace usb_driver
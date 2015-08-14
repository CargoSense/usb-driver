#include "usb_driver.h"

#import <Foundation/Foundation.h>

namespace usb_driver {

void Unmount(const std::string& volume) {
  NSLog(@"USB DRIVER: UNMOUNT VOLUME: %s", volume.c_str());
}

}  // namespace usb_driver
#include "../usb_driver.h"
#include <sys/param.h>
#include <sys/mount.h>

#import <Foundation/Foundation.h>

namespace usb_driver {

bool Unmount(const std::string& volume) {
  NSLog(@"USB DRIVER: UNMOUNT VOLUME: %s", volume.c_str());
  return false;
}

void GetDevice(const std::string& identifier) {
  NSLog(@"USB DRIVER: GET DEVICE: %s", identifier.c_str());
}

void GetDevices() {
  NSLog(@"USB DRIVER: GET DEVICES");
}

}  // namespace usb_driver
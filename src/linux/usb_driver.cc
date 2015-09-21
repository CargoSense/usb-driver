#include "../usb_driver.h"

namespace usb_driver {

bool
Unmount(const std::string &identifier)
{
    // TODO
    return false;
}

struct USBDrive *
GetDevice(const std::string &identifier)
{
    // TODO
    return NULL;
}

std::vector<struct USBDrive *>
GetDevices(void)
{
    // TODO
    std::vector<struct USBDrive *> devices;
    return devices;
}

void
RegisterWatcher(USBWatcher *_watcher)
{
    // TODO
}

void
WaitForEvents(void)
{
    // TODO
}

}  // namespace usb_driver

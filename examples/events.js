var USBDriver = require('../src/usb-driver.js')();

USBDriver.on("attach", function(device) {
  console.log("Attached!");
  console.log(device);
});

USBDriver.on("detach", function(device) {
  console.log("Detached!");
  console.log(device);
});

USBDriver.on("mount", function(device) {
  console.log("Mounted!");
  console.log(device);
});

USBDriver.on("unmount", function(device) {
  console.log("Unmounted!");
  console.log(device);
});

USBDriver.waitForEvents();

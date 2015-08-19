var USBDrive = require('../src/main.js')();

USBDrive.on("attach", function(device) {
  console.log("Attached!");
  console.log(device);
});

USBDrive.on("detach", function(device) {
  console.log("Detacehd!");
  console.log(device);
});

USBDrive.on("mount", function(device) {
  console.log("Mounted!");
  console.log(device);
});

USBDrive.on("unmount", function(device) {
  console.log("Unmounted!");
  console.log(device);
});

USBDrive.waitForEvents();

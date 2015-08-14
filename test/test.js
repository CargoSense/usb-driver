var USBDrive = require('../src/main.js');

USBDrive.getAll()
  .then(function(usbDrives) {
    console.log("GetAll:");
    console.log(usbDrives)
  });

USBDrive.get("1234")
  .then(function(usbDrive) {
    console.log("Get");
    console.log(usbDrive)
  });

USBDrive.unmount("/Volumes/Untitled")
  .then(function() {
    console.log("Unmounted");
  })
  .catch(function() {
    console.log("Not Unmounted");
  });

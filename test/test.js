var USBDriver = require('../src/usb-driver.js')();

USBDriver.getAll()
  .then(function(usbDrives) {
    console.log("GetAll: "+usbDrives.length);
    console.log(usbDrives)
    for(var i = 0;i<usbDrives.length;i++) {
      USBDriver.get(usbDrives[i].id)
        .then(function(usbDrive) {
          if (usbDrive) {
            console.log("Get: "+usbDrive.id);
            console.log(usbDrive)
            if (usbDrive.mount) {
              USBDriver.unmount(usbDrive.mount)
                .then(function() {
                  console.log("Unmounted");
                })
                .catch(function() {
                  console.log("Not Unmounted");
                });
            }
          } else {
            console.log("Could not get "+usbDrives[i])
          }
        });
    }
  });

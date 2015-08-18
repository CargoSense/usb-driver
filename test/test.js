var USBDrive = require('../src/main.js');

USBDrive.getAll()
  .then(function(usbDrives) {
    console.log("GetAll: "+usbDrives.length);
    console.log(usbDrives)
    for(var i = 0;i<usbDrives.length;i++) {
      USBDrive.get(usbDrives[i].id)
        .then(function(usbDrive) {
          if (usbDrive) {
            console.log("Get: "+usbDrive.id);
            console.log(usbDrive)
            if (usbDrive.mount) {
              USBDrive.unmount(usbDrive.mount)
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


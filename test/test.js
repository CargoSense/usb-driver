var USBDrive = require('../src/main.js');

USBDrive.getAll()
  .then(function() {
    console.log("GetAll");
  });

USBDrive.get("1234")
  .then(function() {
    console.log("Get");
  });

USBDrive.unmount("/Volumes/Untitled")
  .then(function() {
    console.log("Unmounted");
  })
  .catch(function() {
    console.log("Not Unmounted");
  });

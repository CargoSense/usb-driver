var USBDrive = require('../src/main.js');

USBDrive.unmount("/Volumes/Untitled")
  .then(function() {
    console.log("Unmounted");
  })
  .catch(function() {
    console.log("Not Unmounted");
  });

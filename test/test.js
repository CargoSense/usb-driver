var USBDrive = require('../src/main.js');

USBDrive.unmount("/Volumes/Untitled")
  .then(function(result) {
    console.log("Unmounted");
  })
  .catch(function(error) {
    console.log("Not Unmounted");
  });

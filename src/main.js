var Promise = require('bluebird');
var DiskWatcher = require('./diskwatcher')();

USBDriver = require('../build/Release/usb_driver.node')

module.exports = {

  on: function(event, callback) {
    DiskWatcher.on(event, callback);
  },
  
  getAll: function() {
    return new Promise(function(resolve, reject) {
      // Return devices list
      resolve([]);
      //reject(false);
    });
  },
  
  get: function(id) {
    return new Promise(function(resolve, reject) {
      // Return device
      resolve(null);
      //reject(false);
    });
  },
  
  unmount: function(volume) {
    return new Promise(function(resolve, reject) {
      USBDriver.unmount(volume);
      resolve(true);
      //reject(false);
    });
  }
};
var Promise = require('bluebird');
var USBDriver = require('../build/Release/usb_driver.node')
var DiskWatcher = require('./diskwatcher')(USBDriver);

/*
Device Object
{
  id: '12:12:WHATEVER',
  vendorCode: '0x0a',
  productCode: '0x12',
  manufacturer: 'Foo Bar Technologies',
  product: 'Baz Sensing Quux',
  serialNumber: 'IDQFB0023AB',
  mount: '/Volumes/FOOBAR1'
}
*/

module.exports = {

  on: function(event, callback) {
    DiskWatcher.on(event, callback);
  },
  
  getAll: function() {
    return new Promise(function(resolve, reject) {
      resolve(USBDriver.getDevices());
    });
  },
  
  get: function(id) {
    return new Promise(function(resolve, reject) {
      resolve(USBDriver.getDevice(id));
    });
  },
  
  unmount: function(volume) {
    return new Promise(function(resolve, reject) {
      if( USBDriver.unmount(volume) ) {
        resolve(true);
      } else {
        reject(false);
      }
    });
  },
  
  waitForEvents: function() {
    USBDriver.waitForEvents();
  }
};
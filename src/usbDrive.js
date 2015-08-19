var Promise = require('bluebird');
var DiskWatcher = require('./diskwatcher')();
var USBDriver = require('../build/Release/usb_driver.node')
USBDriver.registerWatcher(DiskWatcher);

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

var usbDrive;

var USBDrive = function() {
};

USBDrive.prototype.on = function(event, callback) {
  DiskWatcher.on(event, callback);
};

USBDrive.prototype.getAll = function() {
  return new Promise(function(resolve, reject) {
    resolve(USBDriver.getDevices());
  });
};

USBDrive.prototype.get = function(id) {
  return new Promise(function(resolve, reject) {
    resolve(USBDriver.getDevice(id));
  });
};

USBDrive.prototype.unmount = function(volume) {
  return new Promise(function(resolve, reject) {
    if( USBDriver.unmount(volume) ) {
      resolve(true);
    } else {
      reject(false);
    }
  });
};

USBDrive.prototype.waitForEvents = function() {
  console.log("WARNING: This method will not return.")
  USBDriver.waitForEvents();
};

module.exports = function() {
  if (!usbDrive) {
    usbDrive = new USBDrive();
  }
  return usbDrive;
}
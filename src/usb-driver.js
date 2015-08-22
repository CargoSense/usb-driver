var Promise = require('bluebird');
var DiskWatcher = require('./diskwatcher')();
var USBNativeDriver = require('../build/Release/usb_driver.node')
USBNativeDriver.registerWatcher(DiskWatcher);

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

var usbDriver;

var USBDriver = function() {
};

USBDriver.prototype.on = function(event, callback) {
  DiskWatcher.on(event, callback);
};

USBDriver.prototype.getAll = function() {
  return new Promise(function(resolve, reject) {
    resolve(USBNativeDriver.getDevices());
  });
};

USBDriver.prototype.get = function(id) {
  return new Promise(function(resolve, reject) {
    resolve(USBNativeDriver.getDevice(id));
  });
};

USBDriver.prototype.unmount = function(deviceId) {
  return new Promise(function(resolve, reject) {
    if (USBNativeDriver.unmount(deviceId)) {
      resolve(true);
    } else {
      reject();
    }
  });
};

USBDriver.prototype.waitForEvents = function() {
  console.log("WARNING: This method will not return.")
  USBNativeDriver.waitForEvents();
};

module.exports = function() {
  if (!usbDriver) {
    usbDriver = new USBDriver();
  }
  return usbDriver;
}

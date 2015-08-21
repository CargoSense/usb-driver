var util = require('util'),
    EventEmitter = require('events').EventEmitter;

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

var DiskWatcher = function() {
  var self = this;
  this.mountMap = {};
};
util.inherits(DiskWatcher, EventEmitter);

DiskWatcher.prototype.attach = function(device) {
  if (device == null) {return;}
  this.emit('attach', device);
};
DiskWatcher.prototype.detach = function(device) {
  if (device == null) {return;}
  if (this.mountMap[device.id]) {
    this.unmount(device);
  }
  this.emit('detach', device);
};
DiskWatcher.prototype.mount = function(device) {
  if (device == null) {return;}
  this.mountMap[device.id] = true;
  this.emit('mount', device);
};
DiskWatcher.prototype.unmount = function(device) {
  if (device == null) {return;}
  delete this.mountMap[device.id];
  this.emit('unmount', device);
};

function diskWatcher() {
  return new DiskWatcher();
}

module.exports = diskWatcher
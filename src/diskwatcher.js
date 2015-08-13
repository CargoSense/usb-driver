var util = require('util'),
    EventEmitter = require('events').EventEmitter;

var DiskWatcher = function() {
  var self = this;
};
util.inherits(DiskWatcher, EventEmitter);

DiskWatcher.prototype.attach = function(device) {
  this.emit('attach', device);
};
DiskWatcher.prototype.detach = function(device) {
  this.emit('detach', device);
};
DiskWatcher.prototype.mount = function(device) {
  this.emit('mount', device);
};
DiskWatcher.prototype.unmount = function(device) {
  this.emit('unmount', device);
};

function diskWatcher() {
  return new DiskWatcher();
}

module.exports = diskWatcher

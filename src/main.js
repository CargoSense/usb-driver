var Promise = require('bluebird');
var DiskWatcher = require('./diskwatcher')();

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
  
  unmount: function(id) {
    return new Promise(function(resolve, reject) {
      resolve(true);
      //reject(false);
    });
  }
};
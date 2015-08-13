var Promise = require('bluebird');

module.exports = {
  watch: function(callback) {
    retrun null;
  },

  unmount: function(drive) {
    return new Promise(function(resolve, reject) {
      resolve(true);
      //reject(false);
    });
  }
};
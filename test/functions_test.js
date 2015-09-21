var assert = require('chai').use(require('chai-as-promised')).assert;
var proxyquire = require('proxyquire');

describe('usbDriver', function() {

  var usbDriver;

  describe('#getAll()', function () {
    it('should return a promise for an array', function () {
      usbDriver = require('../src/usb-driver')();
      assert.isFulfilled(usbDriver.getAll());
      assert.eventually.isArray(usbDriver.getAll());
    });
  });
  describe('#get()', function () {

    var goodDeviceId = 'good-device-id';
    var nativeStub = {
      registerWatcher: function() {},
      getDevice: function(id) {
        if (id === goodDeviceId) {
          return {id: goodDeviceId};
        } else {
          return null;
        }
      }
    };

    beforeEach(function() {
      usbDriver = proxyquire('../src/usb-driver', {
        '../build/Release/usb_driver.node': nativeStub
      })();
    });
    it('should return a promise for null if a bad deviceId', function () {
      assert.isFulfilled(usbDriver.get('bad-device-id'));
      assert.eventually.isNull(usbDriver.get('bad-device-id'));
    });
    it('should return a promise for an object if a good deviceId', function () {
      assert.isFulfilled(usbDriver.get(goodDeviceId));
      assert.eventually.isObject(usbDriver.get(goodDeviceId));
    });

  });

});

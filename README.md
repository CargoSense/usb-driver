# USB Driver

Cross-platform USB device metadata and events.

## Platforms

* OSX
* Windows
* ~~Linux~~ (Planned)

## Install

```
npm install usb-driver
```

## Usage

```js
var usbDriver = require('usb-driver');
```

### Functions returning promises

#### Get a list of attached devices

Use `getAll()`:

```js
usbDriver.getAll().then(function(devices) {
  /* ... do something with devices ... */
}).catch(function(err) { /* ... error handling */ });
```

`devices` is an array of device objects. See
[Device Objects](#device-objects), below.

#### Get a device by ID

Use `get()`:

```js
usbDriver.get(deviceId).then(function(device) {
  /* ... do something with device ... */
}).catch(function(err) { /* ... error handling ... */ });
```

`deviceId` is the `id` provided in the device objects from
`getAll()`, and `device` is a resulting device object. See
[Device Objects](#device-objects), below.

#### Unmount a (mass storage) device

Use `unmount()`:

Note that the promise will be rejected only if a mounted volume could not be
unmounted by this invocation. If it's already unmounted, the promise
will be resolved.

```js
usbDriver.unmount(deviceId).then(function() {
  /* ... success ... */
}).catch(function(err) { /* ... failure ... */ });
```

`deviceId` is the `id` provided in the device objects from `get()` or
`getAll()`. See [Device Objects](#device-objects), below.

### Events

#### Watch for attach events

```js
usbDriver.on('attach', function(device) { /* ... */ });
```

`device` is a device object. See [Device Objects](#device-objects), below.

#### Watch for detach events

```js
usbDriver.on('detach', function(device) { /* ... */ });
```

`device` is a device object. See [Device Objects](#device-objects), below.

#### Watch for mount events

When a USB device is mounted.

```js
usbDriver.on('mount', function(device) { /* ... */ });
```

`device` is a device object. See [Device Objects](#device-objects), below.

#### Watch for unmount events

When a USB device is unmounted.

```js
usbDriver.on('unmount', function(device) { /* ... */ });
```

`device` is a device object. See [Device Objects](#device-objects), below.

### Device Objects

Device objects represent attached USB devices and model the data about them.

Here's an example:

```js
{
  id: '0x0a-0x12-IDQFB0023AB', // VID-PID-(SERIAL|INCREMENTED_ID)
  vendorCode: '0x0a',
  productCode: '0x12',
  manufacturer: 'Foo Bar Technologies',
  product: 'Baz Sensing Quux',
  serialNumber: 'IDQFB0023AB',
  mount: '/Volumes/FOOBAR1'
}
```

#### id

*REQUIRED*, String | Integer

The id is a unique identifier for an attached device. This is made up
of the product code, vendor code -- and the serial number, if
available. If the serial number is not available, an incremented value
is provided as the last component.

#### vendorCode

*REQUIRED*, String

The hex for the USB vendor ID.

#### productCode

*REQUIRED*, String

The hex for the USB product ID.

#### manufacturer

*OPTIONAL*, String

The name of the manufacturer/vendor, if available as a USB descriptor.

#### product

*OPTIONAL*, String

The name of the product, if available as a USB descriptor.

#### serialNumber

*OPTIONAL*, String

The serial number of the device, if available as a USB descriptor.

#### mount

*OPTIONAL*, String

The path to the volume mount point, if mounted.

## Test

```
npm test
```

## License

See [LICENSE](./LICENSE)

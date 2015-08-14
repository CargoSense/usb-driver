#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <paths.h>
#include <sys/param.h>
#include <mach/mach.h>
#include <mach/error.h>
 
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
 
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOMediaBSDClient.h>
 
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/ioss.h>

char* cfStringRefToCString( CFStringRef cfString )
{
  if ( !cfString ) return NULL;
 
   static char string[2048];
   string[0] = '\0';
   // CFShow( CFCopyDescription( cfString ));
   CFStringGetCString( cfString,
                       string, MAXPATHLEN, GetApplicationTextEncoding() );
   return &string[0];
}
 
char* cfTypeToCString( CFTypeRef cfString )
{
   if ( !cfString ) return NULL;
 
   static char deviceFilePath[2048];
   deviceFilePath[0] = '\0';
   // CFShow( CFCopyDescription( cfString ));
   CFStringGetCString( CFCopyDescription( cfString ),
                       deviceFilePath, MAXPATHLEN, 
                       GetApplicationTextEncoding() );
   char* p = deviceFilePath;
   while ( *p != '\"' ) p++; p++;
   char* pp = p;
   while ( *pp != '\"' ) pp++;
   *pp = '\0';
   if ( isdigit( *p )) *p = 'x';
   return p;
}

void USBDeviceCallback( void* refCon, io_iterator_t iterator, bool inserted );
 
void USBDeviceInserted( void* refCon, io_iterator_t iterator )
{
   USBDeviceCallback( refCon, iterator, true );
}
 
void USBDeviceRemoved( void* refCon, io_iterator_t iterator )
{
   USBDeviceCallback( refCon, iterator, false );
}
 
s32 registerForUSBNotifications()
{
   // Set up matching dictionary for class IOUSBDevice and its subclasses
   CFMutableDictionaryRef matchingDict = IOServiceMatching( "IOUSBDevice" );       
   if ( !matchingDict )
   {
      // could not create dictionary. Should almost never happen.
      return -1;
   }
 
   // Create a master port for communication with the I/O Kit
   mach_port_t masterPort;
   if ( IOMasterPort( MACH_PORT_NULL, &masterPort ) || !masterPort )
   {
      // could not create master port - again this would be highly unexpected.
      return -1;
   }
 
   // To set up asynchronous notifications, create a notification port and
   // add its run loop event source to the program’s run loop
   IONotificationPortRef notifyPort = 
      IONotificationPortCreate( masterPort );
   CFRunLoopSourceRef runLoopSource = 
      IONotificationPortGetRunLoopSource( notifyPort );
   CFRunLoopAddSource( CFRunLoopGetCurrent(), runLoopSource, 
                       kCFRunLoopDefaultMode );
 
   // Retain additional dictionary references because each call to
   // IOServiceAddMatchingNotification consumes one reference
   matchingDict = (CFMutableDictionaryRef) CFRetain( matchingDict );
   matchingDict = (CFMutableDictionaryRef) CFRetain( matchingDict );
   matchingDict = (CFMutableDictionaryRef) CFRetain( matchingDict );
 
   // Now set up two notifications: one to be called when a raw device
   // is first matched by the I/O Kit and another to be called when the
   // device is terminated
 
   // Notification of first match:
   kern_return_t kr;
   io_iterator_t addedIter; 
   kr = IOServiceAddMatchingNotification( notifyPort,
                                          kIOFirstMatchNotification, 
                                          matchingDict,
                                          USBDeviceInserted, 
                                          NULL, 
                                          &addedIter );
   USBDeviceInserted( NULL, addedIter );
   if ( kr )
   {
      // Error in IOServiceAddMatchingNotification
      mach_port_deallocate( mach_task_self(), masterPort );
      return -1;
   }
 
   // Notification of termination:
   io_iterator_t removedIter; 
   kr = IOServiceAddMatchingNotification( notifyPort,
                                          kIOTerminatedNotification, 
                                          matchingDict,
                                          USBDeviceRemoved, 
                                          NULL, 
                                          &removedIter );
   USBDeviceRemoved( NULL, removedIter );
   if ( kr )
   {
      // Error in IOServiceAddMatchingNotification
      mach_port_deallocate( mach_task_self(), masterPort );
      return -1;
   }
 
   mach_port_deallocate( mach_task_self(), masterPort );
 
   return 0;
}

void USBDeviceCallback( void* refCon, io_iterator_t iterator, bool inserted )
{
   io_service_t usbDevice;
   while (( usbDevice = IOIteratorNext( iterator )))
   {
      // this is redundant since we only registered for IOUSBDevices, but 
      // it's a good check, and something you might need if you had registered for 
      // more device types. Eg. If you had registered for notifications to 
      // kIOMediaClass, which includes USB disks, but also other non-usb media -
      // like other disk types, CD-Roms etc.
      if ( !IOObjectConformsTo( usbDevice, "IOUSBDevice" )) continue; 
 
      // you could also do this check by the device's class name:
      io_name_t className;
      IOObjectGetClass( usbDevice, className );
      // fprintf( stderr, "This device's className is %s\n", 
      //          (const char*)className );
      if ( strcmp( className, "IOUSBDevice" )) continue;     
 
      // this is how you get the device name. For USB disks the default
      // name is "USB DISK", but many manufacturers will substitute in their
      // own brand name here. If you wanted to filter by brand you can do it 
      // with the device name.
      io_name_t deviceName;
      IORegistryEntryGetName( usbDevice, deviceName );
      // fprintf( stderr, "Device Name is %s\n", (const char*)deviceName );
      if ( strcmp( deviceName, "Super Disk Brand" ) && // a fake brand example
           strcmp( deviceName, "USB DISK" )) continue; // or any disk.    
 
      // now we look for the mount point for the disk. This will be the unix
      // mount point for the device. It can take some time before when you get
      // the notification and the device is actually mounted, so you have to 
      // check for this device name in a loop, waiting for it to mount.
      CFStringRef bsdName = NULL;
      for ( u32 i = 0; i < 200; i++ )
      {
         bsdName = ( CFStringRef ) IORegistryEntrySearchCFProperty( 
            usbDevice,
            kIOServicePlane,
            CFSTR( kIOBSDNameKey ),
            kCFAllocatorDefault,
            kIORegistryIterateRecursively );
 
        if ( bsdName )
        {
            // The bsdName is a CFStringRef, we'll need to convert it to 
            // a c string, using one of our utility functions, which we can
            // show later.
            char bsdNameBuf[4096];
            sprintf( bsdNameBuf, "/dev/%ss1", cfStringRefToCString( bsdName ));
            const char* bsdNameC = &bsdNameBuf][0];
 
            DASessionRef daSession = DASessionCreate( kCFAllocatorDefault );
            DADiskRef disk = DADiskCreateFromBSDName( kCFAllocatorDefault,
                                                      aSession, bsdNameC );
            if ( disk )
            {
               // once you get here, the device is now mounted, but you
               // have to wait for any associated disk volumes to mount.
               for ( u32 i = 0; i < 200; i++ )
               {
                  CFDictionaryRef desc = DADiskCopyDescription( disk );
                  if ( desc )
                  {
                     // at this point you finally have the volume name associated
                     // with a USB Flash disk. But you get it as a CFTypeRef.
                     // You'll need to convert it to a c string.
                     // CFShow( desc );
                     CFTypeRef str = CFDictionaryGetValue( 
                        desc,
                        kDADiskDescriptionVolumeNameKey );
 
                     // cfTypeToCString will convert it- this is one of our
                     // utility functions that I will show later.
                     char* volumeName = cfTypeToCString( str );
                     if ( volumeName && strlen( volumeName ))
                     {
                        // now that we have the mapping from USB device
                        // to mounted volume, we can finally extract the 
                        // serial number from the USB device, and report it as 
                        // the serial number of a mounted volume. We'll 
                        // show the function to extract this data later.
                        // UsbDeviceInfo is one of our classes that holds 
                        // the data, that we can also describe later.
                        UsbDeviceInfo info;
                        getUSBDeviceInfo( usbDevice, info );
                        info.setVolumeName( volumeName );
 
                        // store the name of the mounted volume with the 
                        // serial number.
                        char* volumePath = malloc( 4096 );
                        sprintf( volumePath, "/Volumes/%s", volumeName );
                        info.setMountPath( volumePath );
 
                        // this calls back to your own code with the volume 
                        // name, serial number and other information.
                        doStuff( info );
 
                        CFRelease( desc );
                        break;
                     } else {
                        CFRelease( desc );
                     }
                  } else {
                     // we didn't get a mounted volume yet - so keep waiting.
                     kxSleep( 100000 );
                  }
               }
               CFRelease( disk );   
            }
            CFRelease( daSession );     
            break;
         } else {
            // we didn't get a bsd name, so keep waiting for the device to
            // get mounted.
            kxSleep( 10000 );
         }
      }
   }
}

void getUSBDeviceInfo( io_service_t usbDevice, UsbDeviceInfo& info )
{
   // Just as ugly as the stuff you have to do for windows
   SInt32                        score; 
   IOCFPlugInInterface**         plugin; 
   IOUSBDeviceInterface182**     usbDevice182 = NULL; 
   kern_return_t                 err; 
  
   err = IOCreatePlugInInterfaceForService( usbDevice, 
                                            kIOUSBDeviceUserClientTypeID, 
                                            kIOCFPlugInInterfaceID, 
                                            &plugin, 
                                            &score ); 
   if ( err != 0 )
   {
      // error getting the plugin interface. You can get this if you somehow
      // found your way here before the device and volumes are fully mounted.
      return;
   }
 
   // Now get the USB device interface. This will allow you to make the low-level
   // USB bus calls to extract device information. This is just a wrapper to
   // the low level USB interface. Once you have one of these, you can manipulate
   // the device at the bus level. BTW "182" means the version of this function.
   // OsX will keep updating this function as their support for new USB bus
   // features is implemented. For our purposes we need 182 or higher.
   err = (*plugin)->QueryInterface( 
      plugin, 
      CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID182), 
      (void**)&usbDevice182 ); 
 
   // we just needed the plugin interface to get the USB interface, so we can 
   // release it now.
   IODestroyPlugInInterface( plugin ); 
 
   if ( err != 0 )
   {
      // failed to get the bus interface.
      return;
   }
 
   // To understand this you should look at the USB bus interface spec. 
   // But basically were getting byte offsets into the USB device descriptor
   // record so we can copy out the strings.
   UInt8 vidIdx;
   UInt8 pidIdx;
   UInt8 snIdx;
   UInt16 rev;
 
   (*usbDevice182)->USBGetManufacturerStringIndex( usbDevice182, &vidIdx );
   (*usbDevice182)->USBGetProductStringIndex     ( usbDevice182, &pidIdx );
   (*usbDevice182)->USBGetSerialNumberStringIndex( usbDevice182, &snIdx );
   (*usbDevice182)->GetDeviceReleaseNumber       ( usbDevice182, &rev );
 
   // copy out the strings and set them in our info class.
   info.setVid          ( getUSBStringDescriptor( usbDevice182, vidIdx ));
   info.setPid          ( getUSBStringDescriptor( usbDevice182, pidIdx ));
   info.setSerialNumber ( getUSBStringDescriptor( usbDevice182, snIdx ));
 
   info.setRev( rev );
}

const char* getUSBStringDescriptor( IOUSBDeviceInterface182** usbDevice, u8 idx )
{
   assert( usbDevice );
   UInt16 buffer[64];
 
   // wow... we're actually forced to make hard coded bus requests. Its like 
   // hard disk programming in the 80's!
   IOUSBDevRequest request;
 
   request.bmRequestType = USBmakebmRequestType( 
      kUSBIn, 
      kUSBStandard, 
      kUSBDevice );
   request.bRequest = kUSBRqGetDescriptor;
   request.wValue = (kUSBStringDesc << 8) | idx;
   request.wIndex = 0x409; // english
   request.wLength = sizeof( buffer );
   request.pData = buffer;
 
   kern_return_t err = (*usbDevice)->DeviceRequest( usbDevice, &request );
   if ( err != 0 )
   {
      // the request failed... fairly uncommon for the USB disk driver, but not
      // so uncommon for other devices. This can also be less reliable if your 
      // disk is mounted through an external USB hub. At this level we actually
      // have to worry about hardware issues like this.
      return NULL;
   }
  
   // we're mallocing this string just as an example. But you probably will want 
   // to do something smarter, like pre-allocated buffers in the info class, or 
   // use a string class.
   char* string = malloc( 128 );
 
   u32 count = ( request.wLenDone - 1 ) / 2;
   u32 i;
   for ( i = 0; i < count; i++ )
      string[i] = buffer[i+1];
   string[i] = '\0';  
 
   return string;
}

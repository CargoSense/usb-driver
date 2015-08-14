class UsbDeviceInfo
{
int revision;
const char* vid;
const char* pid;
const char* serialNumber;
const char* mountPath;
const char* volumeName;

public:
   void setVid( const char* str )
   {
     vid = str;
   }
   void setPid( const char* str )
   {
     pid = str;
   }
   void setRev( int rev )
   {
     revision = rev;
   }
   void setSerialNumber( const char* str )
   {
     serialNumber = str;
   }
   void setMountPath( const char* str )
   {
     mountPath = str;
   }
   void setVolumeName( const char* str )
   {
     volumeName = str;
   }
};
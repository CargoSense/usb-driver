#include "usb_driver.h"
#include <node.h>

namespace node_bindings {
  using v8::FunctionCallbackInfo;
  using v8::Isolate;
  using v8::Local;
  using v8::Handle;
  using v8::Persistent;
  using v8::Exception;

  using v8::String;
  using v8::Boolean;
  using v8::Object;
  using v8::Null;
  using v8::Array;
  using v8::Value;

#define THROW_AND_RETURN(isolate, msg)            \
  do {                                            \
    isolate->ThrowException(Exception::TypeError( \
        String::NewFromUtf8(isolate, msg)));      \
    return;                                       \
  }                                               \
  while(0)

  static Local<Object>
  USBDrive_to_Object(Isolate *isolate, struct usb_driver::USBDrive *usb_drive)
  {
    Local<Object> obj = Object::New(isolate);

#define OBJ_ATTR(name, val)                                         \
    do {                                                            \
      Local<String> _name = String::NewFromUtf8(isolate, name);     \
      if (val.size() > 0) {                                         \
        obj->Set(_name, String::NewFromUtf8(isolate, val.c_str())); \
      }                                                             \
      else {                                                        \
        obj->Set(_name, Null(isolate));                             \
      }                                                             \
    }                                                               \
    while (0)

    OBJ_ATTR("id", usb_drive->uid);
    OBJ_ATTR("productCode", usb_drive->product_id);
    OBJ_ATTR("vendorCode", usb_drive->vendor_id);
    OBJ_ATTR("product", usb_drive->product_str);
    OBJ_ATTR("serialNumber", usb_drive->serial_str);
    OBJ_ATTR("manufacturer", usb_drive->vendor_str);
    OBJ_ATTR("mount", usb_drive->mount);

#undef OBJ_ATTR
    return obj;
  }

  void Unmount(const FunctionCallbackInfo<Value> &args)
  {
    Isolate *isolate = args.GetIsolate();

    if(args.Length() < 1)
      THROW_AND_RETURN(isolate, "Wrong number of arguments");

    if(!args[0]->IsString())
      THROW_AND_RETURN(isolate, "Expected the first argument to by of type string");

    String::Utf8Value utf8_string(args[0]->ToString());
    Local<Boolean> ret;

    if(usb_driver::Unmount(*utf8_string)) {
      ret = Boolean::New(isolate, true);
    } else {
      ret = Boolean::New(isolate, false);
    }

    args.GetReturnValue().Set(ret);
  }

  class NodeUSBWatcher : public usb_driver::USBWatcher {
    Persistent<Object> js_watcher;

  public:
    NodeUSBWatcher(Local<Object> obj) {
      js_watcher.Reset(obj);
    }

    virtual ~NodeUSBWatcher() {
      js_watcher.Reset();
    }

    virtual void
    attached(struct usb_driver::USBDrive *usb_info) {
      emit("attach", usb_info);
    }

    virtual void
    detached(struct usb_driver::USBDrive *usb_info) {
      emit("detach", usb_info);
    }

    virtual void
    mount(struct usb_driver::USBDrive *usb_info) {
      emit("mount", usb_info);
    }

    virtual void
    unmount(struct usb_driver::USBDrive *usb_info) {
      emit("unmount", usb_info);
    }

  private:
    void
    emit(const char *msg, struct usb_driver::USBDrive *usb_info) {
      assert(usb_info != NULL);

      Local<Object> rcv = Nan::New<Object>(js_watcher);
      Handle<Value> argv[1] = { USBDrive_to_Object(usb_info) };

      Nan::MakeCallback(rcv, Nan::New<v8::String>(msg), 1, argv).ToLocalChecked();
    }
  };


  void RegisterWatcher(const FunctionCallbackInfo<Value> &args)
  {
    Local<Object> js_watcher(Local<Object>::Cast(args[0]));
    NodeUSBWatcher *watcher = new NodeUSBWatcher(js_watcher);

    usb_driver::RegisterWatcher(watcher);

    // Return nothing
    args.GetReturnValue().SetNull();
  }

  void WaitForEvents(const FunctionCallbackInfo<Value> &args)
  {
    usb_driver::WaitForEvents();
    // Return nothing
    args.GetReturnValue().SetNull();
  }

  void GetDevice(const FunctionCallbackInfo<Value> &info)
  {
    //Nan::HandleScope scope;
    //String::Utf8Value utf8_string(Local<String>::Cast(args[0]));
    Isolate *isolate = info.GetIsolate();

    String::Utf8Value str(info[0]->ToString());

    auto usb_drive = usb_driver::GetDevice(*str);

    if(usb_drive == NULL) {
      info.GetReturnValue().SetNull();
    } else {
      info.GetReturnValue().Set(USBDrive_to_Object(isolate, usb_drive));
    }
  }

  void GetDevices(const FunctionCallbackInfo<Value> &info)
  {
    auto isolate = info.GetIsolate();
    auto devices = usb_driver::GetDevices();

    Handle<Array> array = Array::New(isolate, devices.size());

    if(array.IsEmpty())
      THROW_AND_RETURN(isolate, "Array creation failed");

    for(size_t i = 0; i < devices.size(); ++i) {
      auto device_obj = USBDrive_to_Object(isolate, devices[i]);

      array->Set((int)i, device_obj);
    }

    info.GetReturnValue().Set(array);
  }

  void
  Init(Handle<Object> exports)
  {
    NODE_SET_METHOD(exports, "unmount", Unmount);
    NODE_SET_METHOD(exports, "getDevice", GetDevice);
    NODE_SET_METHOD(exports, "getDevices", GetDevices);
    NODE_SET_METHOD(exports, "registerWatcher", RegisterWatcher);
    NODE_SET_METHOD(exports, "waitForEvents", WaitForEvents);
  }
}  // namespace

NODE_MODULE(usb_driver, node_bindings::Init)

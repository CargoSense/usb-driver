#include "usb_driver.h"
#include "nan.h"

namespace {
    static inline void setAttr(
        v8::Local<v8::Object> obj,
        const char* name,
        const std::string str)
    {
      v8::Local<v8::String> _name = Nan::New(name).ToLocalChecked();

      if (str.size() > 0) {
        Nan::Set(obj, _name, Nan::New(str).ToLocalChecked());
      } else {
        Nan::Set(obj, _name, Nan::Null());
      }
    }

    static inline v8::Local<v8::Object> USBDrive_to_Object(
        usb_driver::USBDrive *usb_drive)
    {
      v8::Local<v8::Object> obj = Nan::New<v8::Object>();

      setAttr(obj, "id", usb_drive->uid);
      setAttr(obj, "productCode", usb_drive->product_id);
      setAttr(obj, "vendorCode", usb_drive->vendor_id);
      setAttr(obj, "product", usb_drive->product_str);
      setAttr(obj, "serialNumber", usb_drive->serial_str);
      setAttr(obj, "manufacturer", usb_drive->vendor_str);
      setAttr(obj, "mount", usb_drive->mount);

      return obj;
    }

    NAN_METHOD(Unmount) {
      v8::String::Utf8Value utf8_string(v8::Local<v8::String>::Cast(info[0]));

      if (usb_driver::Unmount(*utf8_string)) {
        info.GetReturnValue().Set(Nan::True());
      } else {
        info.GetReturnValue().Set(Nan::False());
      }
    }

    class NodeUSBWatcher : public usb_driver::USBWatcher {
      Nan::Persistent<v8::Object> js_watcher;

      public:
      NodeUSBWatcher(v8::Local<v8::Object> obj) :
        js_watcher(obj) {}

      virtual ~NodeUSBWatcher() {
        js_watcher.Reset();
      }

      virtual void attached(usb_driver::USBDrive *usb_info) {
        emit("attach", usb_info);
      }

      virtual void detached(usb_driver::USBDrive *usb_info) {
        emit("detach", usb_info);
      }

      virtual void mount(usb_driver::USBDrive *usb_info) {
        emit("mount", usb_info);
      }

      virtual void unmount(usb_driver::USBDrive *usb_info) {
        emit("unmount", usb_info);
      }

      private:
      void
        emit(const char *msg, usb_driver::USBDrive *usb_info) {
          assert(usb_info != NULL);

          v8::Local<v8::Object> rcv = Nan::New<v8::Object>(js_watcher);
          v8::Handle<v8::Value> argv[1] = { USBDrive_to_Object(usb_info) };
          Nan::MakeCallback(rcv, Nan::New(msg).ToLocalChecked(), 1, argv);
        }
    };

    NAN_METHOD(RegisterWatcher) {
      v8::Local<v8::Object> js_watcher(v8::Local<v8::Object>::Cast(info[0]));
      NodeUSBWatcher *watcher = new NodeUSBWatcher(js_watcher);
      usb_driver::RegisterWatcher(watcher);
      info.GetReturnValue().Set(Nan::Null());
    }

    NAN_METHOD(WaitForEvents) {
      usb_driver::WaitForEvents();
      info.GetReturnValue().Set(Nan::Null());
    }

    NAN_METHOD(GetDevice) {
      v8::String::Utf8Value utf8_string(v8::Local<v8::String>::Cast(info[0]));
      usb_driver::USBDrive *usb_drive =
        usb_driver::GetDevice(*utf8_string);
      if (usb_drive == NULL) {
        info.GetReturnValue().Set(Nan::Null());
      }
      else {
        info.GetReturnValue().Set(USBDrive_to_Object(usb_drive));
      }
    }

    NAN_METHOD(GetDevices) {
      std::vector<usb_driver::USBDrive *> devices =
        usb_driver::GetDevices();
      v8::Handle<v8::Array> ary = Nan::New<v8::Array>(devices.size());

      for (unsigned int i = 0; i < devices.size(); i++) {
        ary->Set((int)i, USBDrive_to_Object(devices[i]));
      }

      info.GetReturnValue().Set(ary);
    }

    NAN_MODULE_INIT(Init) {
      Nan::Set(target,
               Nan::New("unmount").ToLocalChecked(),
               Nan::GetFunction(Nan::New<v8::FunctionTemplate>(Unmount)).ToLocalChecked());
      Nan::Set(target,
               Nan::New("getDevice").ToLocalChecked(),
               Nan::GetFunction(Nan::New<v8::FunctionTemplate>(GetDevice)).ToLocalChecked());
      Nan::Set(target,
               Nan::New("getDevices").ToLocalChecked(),
               Nan::GetFunction(Nan::New<v8::FunctionTemplate>(GetDevices)).ToLocalChecked());
      Nan::Set(target,
               Nan::New("registerWatcher").ToLocalChecked(),
               Nan::GetFunction(Nan::New<v8::FunctionTemplate>(RegisterWatcher)).ToLocalChecked());
      Nan::Set(target,
               Nan::New("waitForEvents").ToLocalChecked(),
               Nan::GetFunction(Nan::New<v8::FunctionTemplate>(WaitForEvents)).ToLocalChecked());
    }
}  // namespace

NODE_MODULE(usb_driver, Init)

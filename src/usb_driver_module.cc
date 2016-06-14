#include "usb_driver.h"
#include "nan.h"

using namespace v8;

namespace {

    static Local<Object> USBDrive_to_Object(struct usb_driver::USBDrive *usb_drive)
    {
        Local<Object> obj = Nan::New<Object>();

#define OBJ_ATTR(name, val)                                 \
        do {                                                \
            Local<String> _name = Nan::New<v8::String>(name).ToLocalChecked(); \
            if (val.size() > 0) {                           \
                obj->Set(_name, Nan::New<v8::String>(val)).ToLocalChecked();   \
            }                                               \
            else {                                          \
                obj->Set(_name, Nan::Null());                 \
            }                                               \
        }                                                   \
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

    NAN_METHOD(Unmount)
    {
        Nan::HandleScope scope;

        String::Utf8Value utf8_string(Local<String>::Cast(info[0]));
        if (usb_driver::Unmount(*utf8_string)) {
            info.GetReturnValue().Set(Nan::True());
        }
        else {
            info.GetReturnValue().Set(Nan::False());
        }
    }

    class NodeUSBWatcher : public usb_driver::USBWatcher {
        Nan::Persistent<Object> js_watcher;

    public:
        NodeUSBWatcher(Local<Object> obj)
        {
            js_watcher.Reset(obj);
        }

        virtual ~NodeUSBWatcher()
        {
            js_watcher.Reset();
        }

        virtual void attached(struct usb_driver::USBDrive *usb_info)
        {
            emit("attach", usb_info);
        }

        virtual void detached(struct usb_driver::USBDrive *usb_info)
        {
            emit("detach", usb_info);
        }

        virtual void mount(struct usb_driver::USBDrive *usb_info)
        {
            emit("mount", usb_info);
        }

        virtual void unmount(struct usb_driver::USBDrive *usb_info)
        {
            emit("unmount", usb_info);
        }

    private:

        void emit(const char *msg, struct usb_driver::USBDrive *usb_info)
        {
            assert(usb_info != NULL);

            Local<Object> rcv = Nan::New<Object>(js_watcher);
            Handle<Value> argv[1] = { USBDrive_to_Object(usb_info) };
            Nan::MakeCallback(rcv, Nan::New<v8::String>(msg), 1, argv).ToLocalChecked();
        }

    };

    NAN_METHOD(RegisterWatcher)
    {
        Nan::HandleScope scope;
        Local<Object> js_watcher(Local<Object>::Cast(info[0]));
        NodeUSBWatcher *watcher = new NodeUSBWatcher(js_watcher);
        usb_driver::RegisterWatcher(watcher);
        info.GetReturnValue().Set(Nan::Null());
    }

    NAN_METHOD(WaitForEvents)
    {
        Nan::HandleScope scope;
        usb_driver::WaitForEvents();
        info.GetReturnValue().Set(Nan::Null());
    }

    NAN_METHOD(GetDevice)
    {
        Nan::HandleScope scope;

        String::Utf8Value utf8_string(Local<String>::Cast(info[0]));
        struct usb_driver::USBDrive *usb_drive =
            usb_driver::GetDevice(*utf8_string);
        if (usb_drive == NULL) {
            info.GetReturnValue().Set(Nan::Null());
        }
        else {
            info.GetReturnValue().Set(USBDrive_to_Object(usb_drive));
        }
    }

    NAN_METHOD(GetDevices)
    {
        Nan::HandleScope scope;

        std::vector<struct usb_driver::USBDrive *> devices =
            usb_driver::GetDevices();
        Handle<Array> ary = Nan::New<Array>(devices.size());
        for (unsigned int i = 0; i < devices.size(); i++) {
            ary->Set((int)i, USBDrive_to_Object(devices[i]));
        }
        info.GetReturnValue().Set(ary);
    }

    void Init(Handle<Object> exports)
    {
        Nan::SetMethod(exports, "unmount", Unmount);
        Nan::SetMethod(exports, "getDevice", GetDevice);
        Nan::SetMethod(exports, "getDevices", GetDevices);
        Nan::SetMethod(exports, "registerWatcher", RegisterWatcher);
        Nan::SetMethod(exports, "waitForEvents", WaitForEvents);
    }

}  // namespace

NODE_MODULE(usb_driver, Init)

#include "usb_driver.h"
#include "nan.h"

using namespace v8;

namespace {
  NAN_METHOD(Unmount) {
    NanScope();

    String::Utf8Value utf8_string(Local<String>::Cast(args[0]));
    if (usb_driver::Unmount(*utf8_string)) {
      NanReturnValue(NanTrue());
    } else {
      NanReturnValue(NanFalse());
    }
  }

  void Init(Handle<Object> exports) {
    NODE_SET_METHOD(exports, "unmount", Unmount);
  }
}  // namespace

NODE_MODULE(usb_driver, Init)

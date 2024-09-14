# Serial library

*This is the standardized communication between modules using I2C (Wire)*

### File structure:

* `protocol.h`: The base protocol, worked on top of `Wire.h` ("Arduino" library).
* `packaging.h`: The package system implemented and used everywhere in this project between modules. This has callback for secondary modules and tools for master module.
* `flags.h`: Useful flags for `packaging` to work efficiently.

### Example of use (MASTER)

```cpp
auto wire = new CS::PackagedWired(CS::config().set_master()); // you may want to change pins, or LED indicator.

// ...
// in an infinite while loop...
for(uint8_t p = 0; p < CS::d2u(CS::device_id::_MAX); ++p) {
  const auto curr = static_cast<CS::device_id>(p);

  // used to get flags, if answered
  CS::FlagWrapper fw;
  // example of variables to hold the data. Of course you'd use this ...
  // in an object attribute or save in an array, I don't know.
  bool is_online = false;
  bool has_issues = false;
  bool has_new_data = false;

  auto lst = wire->master_smart_request_all(curr, fw, is_online);

  has_issues = fw & CS::device_flags::HAS_ISSUES; // way to get this flag
  has_new_data = fw & CS::device_flags::HAS_NEW_DATA; // in this example ...
  // we don't use this because I am lazy, but you can check if new values were sent.

  if (has_issues || !is_online) continue; // a reason to skip further calls
  if (lst.empty()) continue; // no data sent, so no data to read.
  
  for(const auto& i : lst) {
    switch(i.get_type()) {
    case CS::Command::vtype::TD:
    {
        const auto path = i.get_path(); // path sent
        const auto value = i.get_val<double>(); // value, double
        // do something with it here...
    }
        break;
    case CS::Command::vtype::TF:
    {
        const auto path = i.get_path(); // path sent
        const auto value = i.get_val<float>(); // value, float
        // do something with it here...
    }
        break;
    case CS::Command::vtype::TI:
    {
        const auto path = i.get_path(); // path sent
        const auto value = i.get_val<int64_t>(); // value, int64_t
        // do something with it here...
    }
        break;
    case CS::Command::vtype::TU:
    {
        const auto path = i.get_path(); // path sent
        const auto value = i.get_val<uint64_t>(); // value, uint64_t
        // do something with it here...
    }
        break;
    default: // no match, not useful for us. Skip.
        break;
    }
  }
}
```

### Example of use (SLAVE)

```cpp
// somewhere, once, you do:
auto wire = new PackagedWired(config()
        .set_slave(device_id::MICS_6814_SENSOR)
        .set_slave_callback(callback)
    );

// somewhere else you do this:
void callback(void* rw, const uint8_t expects, const char* received, const uint8_t length)
{
  // Check if the length is the length expected to avoid weird behavior.
  if (length != sizeof(Requester)) return;
  
  PackagedWired& w = *(PackagedWired*) rw;
  Requester req(received);

  // Switch on the offset from master. This will go from 0 to 32-bit size_t, if you allow it to.
  switch(req.get_offset()) {
  case 0: // special case, used to send flags of the device
  {
      FlagWrapper fw;
      if (has_issues())              fw |= device_flags::HAS_ISSUES;
      if (has_new_data_autoreset())  fw |= device_flags::HAS_NEW_DATA;
      
      Command cmd("#FLAGS", (uint64_t)fw); // special case, returns as "#FLAGS"
      w.slave_reply_from_callback(cmd);
  }
  break;
  case 1:
  {
      // I am not even sure if this will work, but this is an example of random number to reply
      const int64_t val = rand();
      // Path is key to make things readable later, as of paths and variable names in logs.
      Command cmd("/my/random", val);
      w.slave_reply_from_callback(cmd);
  }
  break;
  case 2:
  {
      const float val = 3.1415f; // I don't know, you do you
      Command cmd("/my/pi", val);
      w.slave_reply_from_callback(cmd);
  }
  break;
  default: // This is important to make sure master will stop after the previous case. ...
  // Any other case will return as "end" by doing this.
  {
      Command cmd; // "invalid", "end of file", you call it
      w.slave_reply_from_callback(cmd);
  }
  }
}

// this may be useful if you're on Arduino IDE as I am. Just kill this loop thread.
void loop() { vTaskDelete(NULL); }
```

# Serial library

*This is the standardized communication between modules using I2C (Wire)*

### File structure:

* `protocol.h`: The base protocol, worked on top of `Wire.h` ("Arduino" library).
* `packaging.h`: The package system implemented and used everywhere in this project between modules. This has callback for secondary modules and tools for master module.
* `flags.h`: Useful flags for `packaging` to work efficiently.

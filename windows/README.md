# Windows instructions

## Build instructions

### Requirements:

- Microsoft Visual Studio 2022 with C++ support: https://visualstudio.microsoft.com/downloads/ (Community Edition is free)
- Cypress/Infineon FX3 USB Suite for the CyAPI library. It is part of the EZ-USB™ FX3 Software Development Kit: https://www.infineon.com/cms/en/design-support/tools/sdk/usb-controllers-sdk/ez-usb-fx3-software-development-kit/ (free, requires registration as a developer)
- Cypress CyUSB3 device driver, which is included in the EZ-USB™ FX3 Software Development Kit above.

### Build steps:

- install the CyUSB3.sys Windows device driver
- open a new Visual Studio Developer terminal: Start -> Visual Studio 2022 -> Developer Command Prompt for VS22
- in that terminal run these commands:
```
cd streaming-client\windows
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
cd ..
```

The streaming client executable will be under build\Release\streaming-client.exe


## How to run it

Stream using DFC firmware for 20 seconds (sample rate=100MHz):
```
build\Release\streaming-client.exe -f fvtest-dual-adc.img -t 20 -s 100e6
```

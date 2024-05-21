# DFC transceiver


## How to build the FX3 firmware image

Firsts install the latest version of Cypress/Infineon FX3 SDK (1.3.5 at this time)
Then build the firmware:
```
cd fx3-firmware
make
```


## How to build the streaming client for Linux

```
cd streaming-client-linux
mkdir build
cd build
cmake ..
make
cd ..
```


## How to build the streaming client for Windows

Firsts install the latest version of Visual Studio with C++ support and Cypress CyUSB drivers.
Then build the streaming client:
```
cd streaming-client-windows
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
cd ..
```
Detailed notes for Windows are in this [README](streaming-client-windows/README.md)


## How to stream from the DFC transceiver

Stream (RX) from the DFC transceiver for 20 seconds in single ADC mode (default sample rate=32MHz):
```
./streaming-client -f fx3-firmware.img -m 1 -t 20
```

Stream (RX) from the DFC transceiver for 20 seconds in single ADC mode (sample rate=100MHz):
```
./streaming-client -f fx3-firmware.img -m 1 -s 100e6 -t 20
```

Stream (RX) from the DFC transceiver for 20 seconds in dual ADC mode (default sample rate=32MHz):
```
./streaming-client -f fx3-firmware.img -m 2 -t 20
```

Stream (RX) from the DFC transceiver for 20 seconds in dual ADC mode (sample rate=100MHz):
```
./streaming-client -f fx3-firmware.img -m 2 -s 100e6 -t 20
```


## License

Licensed under the GNU GPL V3 (see [LICENSE](LICENSE))

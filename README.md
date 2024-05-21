# DFC streaming client


## Examples

Stream using DFC firmware for 20 seconds in single ADC mode (default sample rate=32MHz):
```
./streaming-client -f fx3-firmware.img -m 1 -t 20
```

Stream using DFC firmware for 20 seconds in single ADC mode (sample rate=100MHz):
```
./streaming-client -f fx3-firmware.img -m 1 -s 100e6 -t 20
```

Stream using DFC firmware for 20 seconds in dual ADC mode (default sample rate=32MHz):
```
./streaming-client -f fx3-firmware.img -m 2 -t 20
```

Stream using DFC firmware for 20 seconds in dual ADC mode (sample rate=100MHz):
```
./streaming-client -f fx3-firmware.img -m 2 -s 100e6 -t 20
```


## License

Licensed under the GNU GPL V3 (see [LICENSE](LICENSE))

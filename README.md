# DFC streaming client


## Examples

Stream using DFC firmware for 20 seconds (default sample rate=32MHz):
```
./streaming-client -f fvtest-slavefifo-si5351-clk.img -t 20
```

Stream using DFC firmware for 20 seconds (sample rate=100MHz):
```
./streaming-client -f fvtest-slavefifo-si5351-clk.img -s 100e6 -t 20
```


## License

Licensed under the GNU GLP V3 (see [LICENSE](LICENSE))

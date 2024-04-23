# DFC streaming client


## Examples

Stream using SDDC firmware for 20 seconds:
```
./streaming-client -f SDDC_FX3.img -t 20
```

Stream using Cypress bulk source sink example firmware for 20 seconds:
```
./streaming-client -f cyfxbulksrcsink.img -C -e 1 -t 20
```

Stream using Cypress GPIF to USB DMA multichannel example firmware for 20 seconds:
```
./streaming-client -f cyfxgpiftousbmulti.img -C -t 20
```


## License

Licensed under the GNU GLP V3 (see [LICENSE](LICENSE))

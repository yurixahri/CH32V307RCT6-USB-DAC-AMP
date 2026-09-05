## CH32V307RTC6-USB-DAC-AMP

![Schematic](./Kicad/ch32v307rct6_dac.svg)

<img width="1381" height="585" alt="image" src="https://github.com/user-attachments/assets/653717d5-043e-4992-a330-43bedd36de6a" />


Build with Mounriver Studio, and flash with WCHISPStudio. To flash firmware, connect device to computer, hold BOOT and press RST button, device will appear in WCHISPStudio, select device and Deprotect it, then press BOOT and Download firmware.
In schematic, i do include R series for TPA6138A2 UVP pin, but please leave it empty when assemble.

## Credits
- https://github.com/LoveLonelyTime/USBAudioToI2S
- https://cdn.promelec.ru/upload/items/2023/10/13/CH32V303-305-307_.PDF
- https://www.ti.com/lit/ds/symlink/tpa6138a2.pdf?ts=1786771209908
- http://www.pavouk.org/hw/audiosystem20/en_pcm5102dac.html
- https://assets.nexperia.com/documents/data-sheet/74LVC1G157.pdf

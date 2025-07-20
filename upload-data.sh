#!/bin/bash
~/.arduino15/packages/esp32/tools/mkspiffs/0.2.3/mkspiffs -c data -b 4096 -p 256 -s 0x160000 spiffs.bin
~/Arduino/esptool/bin/esptool --chip esp32 --port /dev/ttyUSB0 write-flash 0x290000 spiffs.bin

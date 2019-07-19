
## this line is used with the OTA partitions file (different offset)
#mkspiffs -c spiffs_content -b 4096 -p 256 -s 0x170000 spiffs_image.img
#esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash --flash_size detect 0x290000 spiffs_image.img
## this line is used with the debugging partitions file
mkspiffs -c spiffs_content -b 4096 -p 256 -s 0x1F0000 spiffs_image.img
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash --flash_size detect 0x210000 spiffs_image.img

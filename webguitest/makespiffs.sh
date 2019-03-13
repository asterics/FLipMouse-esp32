mkspiffs -c spiffs_content -b 4096 -p 256 -s 2031612 spiffs_image.img
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash --flash_size detect 0x210000 spiffs_image.img

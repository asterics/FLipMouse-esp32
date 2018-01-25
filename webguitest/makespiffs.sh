mkspiffs -c spiffs_content -b 8192 -p 256 -s 1048576 spiffs_image.img
python ~/esp/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash --flash_size detect 0x180000 spiffs_image.img

#set paths according to FABI/FM
if [ "$1" = "FM" ]; then
    IMAGEFILE="spiffs_FM.img"
    SRCPATH="./minified_gz/flipmouse"
    SPIFFSFILES="./spiffs_content/flipmouse/*"
    echo "Compiling for FLipMouse"
elif [ "$1" = "FABI" ]; then
    IMAGEFILE="spiffs_FABI.img"
    SRCPATH="./minified_gz/fabi"
    SPIFFSFILES="./spiffs_content/fabi/*"
    echo "Compiling for FABI"
else
    echo "Select either FM or FABI!"
    exit
fi

if [ "$2" = "" ]; then
    echo "Please select an USB port!"
    exit
fi

#minify sources
cd ..
npm install
cd webgui

#remove old gz files & create new ones
rm -r ./minified_gz
cp -r ./minified ./minified_gz
cd minified_gz
find . -type f ! -name '*.gz' -exec gzip "{}" \;
cd ..

#update additional SPIFFS files (config/settings)
cp -r $SPIFFSFILES $SRCPATH


## this line is used with the OTA partitions file (different offset)
#mkspiffs -c $SRCPATH -b 4096 -p 256 -s 0x170000 $IMAGEFILE
#esptool.py --chip esp32 --port $2 --baud 921600 write_flash --flash_size detect 0x290000 $IMAGEFILE
## this line is used with the debugging partitions file
mkspiffs -c $SRCPATH -b 4096 -p 256 -s 0x1F0000 $IMAGEFILE
esptool.py --chip esp32 --port $2 --baud 921600 write_flash --flash_size detect 0x210000 $IMAGEFILE

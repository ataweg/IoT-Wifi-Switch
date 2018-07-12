# IoT-Wifi-Switch
ESP8266 (NodenMCU) IoT-Wifi-Switch with Web interface and MQTT support

I use the ESP8266_NONOS_SDK, the SysGCC toolchain (http://gnutoolchains.com/esp8266/) and GNU Make 4.2.1

To build the firmware:
Edit the Makefile
* Set the variable BUILDDIR to the correct path for the build directory.
* Set the variables for the COM port to flash the target.

On a Windows host open a DOS window:
* set the PATH variable: e.g. PATH=C:\msys32\usr\bin;C:\Python27;%PATH%
* go to the source folder: e.g pushd F:\Projects\InternetOfThings\Devices\IoT-Wifi-Relay\Github\IoT-Wifi-Switch

make clean
cls&make -s TARGET_DEVICE=NodeMCU
cls&make -s TARGET_DEVICE=NodeMCU flash
cls&make -s TARGET_DEVICE=NodeMCU flash&putty -load "COM8_115200_8N1"
cls&make -s TARGET_DEVICE=NodeMCU flashhtml

cls&make -s TARGET_DEVICE=NodeMCU DEBUG_MEMLEAK=yes DEBUG_HEAP_INFO=yes flash&putty -load "COM8_115200_8N1"

C:\Espressif\esptool\esptool.py --port COM8 --baud 460800 erase_flash

Setup WLAN Access Point on Windows Host
---------------------------------------
netsh wlan stop hostednetwork
netsh wlan set hostednetwork mode=allow ssid=esp-wlan key=1234567890 keyUsage=persistent
netsh wlan start hostednetwork


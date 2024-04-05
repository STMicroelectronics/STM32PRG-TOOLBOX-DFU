In order to utilize the STM32 USB DFU device without needing sudo permission,
it is necessary to copy the udev file. This file contains the rules that allow the device to be accessed by non-root user.

Files to copy in /etc/udev/rules.d/ on Ubuntu ("sudo cp *.* /etc/udev/rules.d").
#!/bin/sh
cd "$(dirname "$0")"
dfu-programmer at32uc3b0256 erase
dfu-programmer at32uc3b0256 flash ansible.hex --suppress-bootloader-mem
dfu-programmer at32uc3b0256 start	

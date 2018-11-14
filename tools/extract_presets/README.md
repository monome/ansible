# Extracting Ansible presets to a JSON file

Get into bootloader mode:

* turn off ansible
* unplug the USB A-A cable from ansible
* hold down the preset/mode button while powering on ansible
* plug in the USB A-A cable

Dump the hex file:

``` bash
dfu-programmer at32uc3b0512 read > ansible-backup.hex
```

Run the script:

``` bash
python -m virtualenv .
source Scripts/activate
pip install -r requirements.pip
python extract_presets.py ansible-backup.hex --version 1.6.1 --out ansible-presets.json
```

The preset format from the module's flash is different depending on
your firmware version, so you need to specify the firmware version the
hexdump came from with the `--version` switch if different from the
default (1.6.1). Support for a different firmware version is
straightforward to add, see e.g. schemata/v161.py.



# This part doesn't work yet

Load ansible-presets.json on the root of a USB disk. The JSON format
is human-readable-ish, you can fiddle with it first if you want.

Plug the USB disk into (running, updated) Ansible.

To load presets from the ansible-presets.json file to the module's flash:
* Tap KEY 1. The LED turns white.
* Tap KEY 1 again to confirm overwriting your presets in
flash. The LED blinks white while reading the preset from the USB
disk, then turns off when the preset is loaded.

To save presets from the module's flash to ansible-presets.json:
* Tap KEY 2. The LED turns orange.
* Tap KEY 2 again to confirm overwriting the presets file on the USB disk. The LED blinks orange while writing the preset to the USB disk, then turns off when the preset is saved.

Tap the MODE key to cancel/exit USB disk mode.
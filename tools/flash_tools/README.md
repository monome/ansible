# flash_tools

Gadgets for hacking on monome firmware images.

# Setup

Needs Python 3.5-ish.

``` bash
python -m virtualenv .
source Scripts/activate
pip install -r requirements.pip
```

# Tools

## repl - firmware file inspection

``` bash
python main.py teletype repl --version 3.0.0 teletype-backup.hex
```

will drop you to a Python shell. Variables in scope are:

* `ffi` - A [CFFI](https://cffi.readthedocs.io/en/latest/) instance
  for working with C data structures, with the definitions in scope
  for the flash data structure of firmware/version you specify.

* `ih` - An
  [IntelHex](https://python-intelhex.readthedocs.io/en/latest/)
  instance with the specified hex file loaded.

* `flash` - A `bytes` with the whole contents of flash.

* `nvram_data` - The C structure from flash represented as a python
  object (built by CFFI).

* `hexdump` - A [function](https://pypi.org/project/hexdump/) for
  formatting bytestrings as more readable hex dumps.


## extract - convert presets from a hexdump to a JSON file

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
python main.py ansible extract --version 1.6.1 ansible-backup.hex --out ansible-presets.json
```

The preset format from the module's flash is different depending on
your firmware version, so you need to specify the firmware version the
hexdump came from with the `--version` switch if different from the
default (1.6.1). Support for a different firmware version is
straightforward to add, see e.g. schemata/v161.py.



## docdef - generate data structures describing C structs as JSON

This is the flakiest one and will require some manual editing of the
result, docs and cleanup forthcoming.
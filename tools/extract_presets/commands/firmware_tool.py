import faulthandler

from cffi import FFI
from intelhex import IntelHex

from preset_schemata import PRESET_SCHEMATA

class FirmwareTool:
    
    def __init__(self, firmware, version, hexfile=None):
        self.ffi = FFI()
        try:
            self.schema = PRESET_SCHEMATA[firmware][version](self.ffi)
        except KeyError:
            raise NotImplementedError("don't know how to read version {}".format(version))
        else:
            self.ffi.cdef(self.schema.cdef())

        if hexfile is not None:
            self.ih = IntelHex()
            self.ih.fromfile(hexfile, format='hex')

            # without this, the program will just silently exit if it
            # segfaults trying to read values from the CFFI object
            faulthandler.enable()

            self.nvram_data = self.ffi.new('{} *'.format(self.schema.root_type()))
            nvram_buffer = self.ffi.buffer(self.nvram_data)

            # address from the ansible.sym symbol table
            nvram_dump = self.ih.tobinarray(
                self.schema.address(),
                self.schema.address() + len(nvram_buffer) - 1
            )
            nvram_buffer[:] = nvram_dump


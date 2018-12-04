import code

from commands.firmware_tool import FirmwareTool
from hexdump import hexdump


class ReadEvalPrintLoop(FirmwareTool):
    def run(self):
        flash = self.ih.tobinarray(*self.ih.segments()[1])
        print('flash segment: 0x{:02X} bytes'.format(len(flash)))
        print(' nvram_data_t: 0x{:02X} bytes'.format(self.ffi.sizeof(self.nvram_data[0])))
        code.interact(local={
            'ffi': self.ffi,
            'flash': flash,
            'nvram_data': self.nvram_data,
            'hexdump': hexdump,
        })

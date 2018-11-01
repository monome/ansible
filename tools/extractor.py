from cffi import FFI
from intelhex import IntelHex

from ansible_cdefs import CDEFS


class PresetExtractor:
    def __init__(self, hexfile, version):
        self.ih = IntelHex()
        self.ih.fromfile(hexfile, format='hex')
        self.ffi = FFI()
        try:
            cdef = CDEFS[version]
        except KeyError:
            raise NotImplementedError("don't know how to read version {}".format(version))
        else:
            self.ffi.cdef(cdef)

    def extract(self):
        flash_range = self.ih.segments()[1]
        nvram_data = self.ffi.cast(
            'nvram_data_t *',
            self.ffi.from_buffer(
                self.ih.tobinarray(*flash_range)
            )
        )
        import pdb; pdb.set_trace()

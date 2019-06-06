import faulthandler

from cffi import FFI
from intelhex import IntelHex

class PresetExtractor:
    def __init__(self, schemata, hexfile, version, target_version=None):
        self.ih = IntelHex()
        self.ih.fromfile(hexfile, format='hex')
        self.ffi = FFI()
        try:
            self.schema = schemata[version](self.ffi)
        except KeyError:
            raise NotImplementedError("don't know how to read version {}".format(version))
        else:
            self.ffi.cdef(self.schema.cdef())
        self.target_version = target_version or self.schema.LATEST_VERSION

    def extract(self):
        # without this, the program will just silently exit if it
        # segfaults trying to read values from the CFFI object
        faulthandler.enable()

        nvram_data = self.ffi.new('{} *'.format(self.schema.root_type()))
        nvram_buffer = self.ffi.buffer(nvram_data)
        nvram_dump = self.ih.tobinarray(
            self.schema.address(),
            self.schema.address() + len(nvram_buffer) - 1
        )
        nvram_buffer[:] = nvram_dump

        if not self.schema.check(nvram_data):
            quit()

        preset = {
            'meta': {
                'firmware': self.schema.firmware_name(),
                'version': self.target_version,
                **self.schema.meta(nvram_data),
            },
            'shared': self.schema.shared(nvram_data),
            'apps': {
                app: self.extract_app_state(nvram_data, app)
                for app in self.schema.app_list()
            },
        }

        return (preset, nvram_data)

    def extract_app_state(self, nvram_data, app_name):
        extractor = getattr(self.schema, 'extract_{}_state'.format(app_name))
        state = getattr(nvram_data, '{}_state'.format(app_name))
        return extractor(state)

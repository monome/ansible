import faulthandler

from cffi import FFI
from intelhex import IntelHex

from preset_schemata import PRESET_SCHEMATA


class PresetExtractor:
    target_version = '1.6.1-dev'

    def __init__(self, hexfile, version):
        self.ih = IntelHex()
        self.ih.fromfile(hexfile, format='hex')
        self.ffi = FFI()
        try:
            self.schema = PRESET_SCHEMATA[version](self.ffi)
        except KeyError:
            raise NotImplementedError("don't know how to read version {}".format(version))
        else:
            self.ffi.cdef(self.schema.cdef())

    def extract(self):
        # without this, the program will just silently exit if it
        # segfaults trying to read values from the CFFI object
        faulthandler.enable()

        nvram_data = self.ffi.new('nvram_data_t *')
        nvram_buffer = self.ffi.buffer(nvram_data)

        # address from the ansible.sym symbol table
        nvram_dump = self.ih.tobinarray(
            0x80040000,
            0x80040000 + len(nvram_buffer) - 1
        )
        nvram_buffer[:] = nvram_dump

        if nvram_data.fresh != 0x22:
            print("this firmware image hasn't ever been run, no preset to extract")
            quit()

        preset = {
            'meta': {
                'firmware': 'ansible',
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

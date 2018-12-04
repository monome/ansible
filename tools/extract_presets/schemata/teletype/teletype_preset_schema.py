from preset_schema import PresetSchema


class TeletypePresetSchema(PresetSchema):
    def firmware_name(self):
        return 'teletype'
    
    def check(self, nvram_data):
        if nvram_data.fresh != 0x22:
            print("this firmware image hasn't ever been run (or is corrupt)")
            return False
        return True

    def root_type(self):
        return 'nvram_data_t'

    def address(self):
        return 0x80040000

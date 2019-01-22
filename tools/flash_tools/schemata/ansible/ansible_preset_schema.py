from preset_schema import PresetSchema


class AnsiblePresetSchema(PresetSchema):
    LATEST_VERSION = '1.6.1-dev'

    def firmware_name(self):
        return 'ansible'

    def address(self):
        # from the ansible.sym symbol table
        return 0x80040000

    def check(self, nvram):
        if nvram.fresh != 0x22:
            print("this firmware image hasn't ever been run (or is corrupt)")
            return False
        return True

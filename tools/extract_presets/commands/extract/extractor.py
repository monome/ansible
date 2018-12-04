from preset_schemata import PRESET_SCHEMATA
from commands.firmware_tool import FirmwareTool


class PresetExtractor(FirmwareTool):
    target_version = '1.6.1-dev'

    def extract(self):
        if not self.schema.check(self.nvram_data):
            quit()

        preset = {
            'meta': {
                'firmware': 'ansible',
                'version': self.target_version,
                'i2c_addr': self.nvram_data.state.i2c_addr,
            },
            'shared': {
                'scales': [
                    self.schema.encode_buffer(scale)
                    for scale in self.nvram_data.scale
                ],
            },
            'apps': {
                app: self.extract_app_state(self.nvram_data, app)
                for app in self.schema.app_list()
            },
        }

        return (preset, self.nvram_data)

    def extract_app_state(self, nvram_data, app_name):
        extractor = getattr(self.schema, 'extract_{}_state'.format(app_name))
        state = getattr(self.nvram_data, '{}_state'.format(app_name))
        return extractor(state)

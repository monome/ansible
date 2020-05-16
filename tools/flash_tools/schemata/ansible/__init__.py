from schemata.ansible.v161 import PresetSchema_v161
from schemata.ansible.v161_es import PresetSchema_v161_es
from schemata.ansible.vnext import PresetSchema_vnext
from schemata.ansible.v300 import PresetSchema_v300

ANSIBLE_SCHEMATA = {
    '1.6.1': PresetSchema_v161,
    '1.6.1-es': PresetSchema_v161_es,
    'next': PresetSchema_vnext,
    '3.0.0': PresetSchema_v300,
}

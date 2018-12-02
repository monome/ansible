import argparse
import json

from extractor import PresetExtractor
from schemata.ansible.ansible_preset_schemata import ANSIBLE_PRESET_SCHEMATA


def extract(args):
    extractor = PresetExtractor(
        ANSIBLE_PRESET_SCHEMATA,
        args.hexfile,
        args.version,
        args.target_version,
    )
    presets, image = extractor.extract()
    with open(args.out, 'w') as outf:
        outf.write(json.dumps(
            presets,
            indent=4 if args.pretty else None
        ))
    print('{} preset written to {}'.format(extractor.target_version, args.out))

def command(parser):
    parser.add_argument(
        'hexfile',
        type=str,
        help='name of the hex dump file to inspect'
    )
    parser.add_argument(
        '--version',
        type=str,
        help='firmware version of the ansible which saved the preset',
        default='1.6.1'
    )
    parser.add_argument(
        '--target_version',
        type=str,
        help='firmware version to target with the JSON output'
    )
    parser.add_argument(
        '--out',
        type=str,
        help='JSON file to write the preset to',
        default='ansible-presets.json'
    )
    parser.add_argument(
        '--pretty',
        action='store_true',
        help='pretty-print the JSON output',
        default=False,
    )
    parser.set_defaults(func=extract)

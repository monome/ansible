import argparse
import json

from commands.extract.extractor import PresetExtractor


def extract(args):
    extractor = PresetExtractor(firmware, args.version, args.hexfile)
    presets, image = extractor.extract()
    with open(args.out, 'w') as outf:
        outf.write(json.dumps(
            presets,
            indent=4 if args.pretty else None,
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

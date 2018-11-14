import argparse
import json

from extractor import PresetExtractor


def main():
    parser = argparse.ArgumentParser()
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
    args = parser.parse_args()

    extractor = PresetExtractor(args.hexfile, args.version)
    presets, image = extractor.extract()
    with open(args.out, 'w') as outf:
        outf.write(json.dumps(presets))


if __name__ == '__main__':
    main()

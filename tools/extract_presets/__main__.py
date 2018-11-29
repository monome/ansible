import argparse

import extract_presets
import write_docdef


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers()

    extract_parser = subparsers.add_parser('extract')
    extract_presets.command(extract_parser)

    docdef_parser = subparsers.add_parser('docdef')
    write_docdef.command(docdef_parser)

    args = parser.parse_args()
    args.func(args)


if __name__ == '__main__':
    main()

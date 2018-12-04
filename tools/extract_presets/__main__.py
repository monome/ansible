import argparse

from commands.extract import extract_presets
from commands.docdef import write_docdef
from commands.repl import repl


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'firmware',
        type=str,
        help='name of the firmware to work with'
    )

    subparsers = parser.add_subparsers()

    extract_parser = subparsers.add_parser('extract')
    extract_presets.command(extract_parser)

    docdef_parser = subparsers.add_parser('docdef')
    write_docdef.command(docdef_parser)

    repl_parser = subparsers.add_parser('repl')
    repl.command(repl_parser)

    args = parser.parse_args()
    args.func(args)


if __name__ == '__main__':
    main()

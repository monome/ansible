import argparse
import json

from commands.repl.read_eval_print_loop import ReadEvalPrintLoop


def extract(args):
    repl = ReadEvalPrintLoop(args.firmware, args.version, args.hexfile)
    repl.run()
    
def command(parser):
    parser.add_argument(
        'hexfile',
        type=str,
    )
    parser.add_argument(
        '--version',
        type=str,
        help='firmware version to work with',
        default='1.6.1'
    )
    parser.set_defaults(func=extract)

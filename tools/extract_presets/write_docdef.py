from docdef_writer import DocdefWriter


def write(args):
    writer = DocdefWriter(args.version)
    with open(args.out, 'w') as outf:
        writer.write(outf)

def command(parser):
    parser.add_argument(
        '--version',
        type=str,
        help='firmware version to emit a preset document definition for',
        default='1.6.1'
    )
    parser.add_argument(
        '--out',
        type=str,
        help='file to write generated C code to',
        default='ansible_docdef.inc'
    )
    parser.set_defaults(func=write)

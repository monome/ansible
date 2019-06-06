from commands.docdef.docdef_writer import DocdefWriter


def write(args):
    writer = DocdefWriter(args.name, args.firmware, args.version)
    with open(args.out, 'w') as outf:
        writer.write(outf)
    print('wrote document defintion data structure to {}'.format(args.out))


def command(parser):
    parser.add_argument(
        'name',
        type=str,
        help='name of the variable to declare',
    )
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
        default='ansible_docdef.c'
    )
    parser.set_defaults(func=write)

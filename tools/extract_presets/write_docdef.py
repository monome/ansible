from docdef_writer import DocdefWriter
from schemata.ansible.ansible_preset_schemata import ANSIBLE_PRESET_SCHEMATA


def write(args):
    writer = DocdefWriter(
        ANSIBLE_PRESET_SCHEMATA,
        args.name,
        args.version
    )
    with open(args.out, 'w') as outf:
        writer.write(outf)

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

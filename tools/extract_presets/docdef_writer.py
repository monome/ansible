from contextlib import contextmanager

from cffi import FFI

from preset_schemata import PRESET_SCHEMATA





class DocdefWriter:
    INDENT_STR = '\t'
    FIRMWARE_NAME = 'ansible'
    ROOT_TYPE = 'nvram_data_t'

    def __init__(self, version):
        self.indentation = 0
        self.obj_depth = -1
        self.ffi = FFI()
        try:
            self.schema = PRESET_SCHEMATA[version](self.ffi)
        except KeyError:
            raise NotImplementedError("don't know struct defs for version {}".format(version))
        else:
            self.ffi.cdef(self.schema.cdef())

    @contextmanager
    def indent(self, out, brackets=None):
        self.indentation += 1
        if brackets:
            out.write(brackets[0] + '\n')
        yield
        self.indentation -= 1
        self.put_indented(out, brackets[1])

    def put_indented(self, out, s):
        out.write(self.indentation * self.INDENT_STR + s)
        
    def putln(self, out, line):
        out.write(self.indentation * self.INDENT_STR + line + '\n')
        
    def write(self, out):
        nvram_t = self.ffi.typeof('nvram_data_t')
        self.putln(
            out,
            'preset_section_handler_t {}_handler = '.format(self.FIRMWARE_NAME)
        )
        self.write_object(out, nvram_t)
        out.write(';\n')

    def write_object(self, out, ctype):
        self.obj_depth += 1
        with self.indent(out, ('{', '}')):
            self.putln(out, '.read = load_object,')
            self.putln(out, '.read = save_object,')
            self.putln(out, '.fresh = true,')
            self.putln(
                out,
                '.state = &json_object_state[{}],'.format(self.obj_depth),
            )
            
            self.put_indented(out, '.params = &')
            with self.indent(out, ('((load_object_params_t) {', '}),\n')):
                self.putln(out, '.handler_ct = {},'.format(len(ctype.fields)))
        self.obj_depth -= 1

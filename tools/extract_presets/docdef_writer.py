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
        self.arr_depth = -1
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

    def write_object(self, out, ctype, name=None):
        self.obj_depth += 1
        with self.indent(out, ('{', '}')):
            if name is not None:
                self.putln(out, '.name = {},'.format(name))
            self.putln(out, '.read = load_object,')
            self.putln(out, '.read = save_object,')
            self.putln(out, '.fresh = true,')
            self.putln(
                out,
                '.state = &json_object_state[{}],'.format(self.obj_depth),
            )

            with self.write_params(out, 'load_object_params_t'):
                self.putln(out, '.handler_ct = {},'.format(len(ctype.fields)))
                self.put_indented(out, '.handlers = ')
                with self.indent(
                    out,
                    (
                        '((preset_section_handler_t[]) {',
                        '}),\n'
                    )
                ):
                    for field_name, field in ctype.fields:
                        kind_writer = self.get_writer(field.type.kind)
                        self.put_indented(out, '')
                        with self.indent(out, ('{', '},\n')):
                            self.putln(out, '.name = "{}",'.format(field_name))
                            kind_writer(out, field_name, field)

        self.obj_depth -= 1

    def get_writer(self, kind):
        return getattr(self, 'write_{}'.format(kind))

    def write_primitive(self, out, field_name, field):
        self.putln(out, '.read = load_scalar,')
        if field.type.cname.startswith('uint'):
            self.putln(out, '.write = save_number,')
            with self.write_params(out, 'load_scalar_params_t', field_name):
                pass
            return
        if field.type.cname.startswith('int'):
            self.putln(out, '.write = save_number,')
            with self.write_params(out, 'load_scalar_params_t', field_name):
                self.putln('.signed_val = true,')
            return
        if field.type.cname == '_Bool':
            self.putln(out, '.write = save_bool,')
            with self.write_params(out, 'load_scalar_params_t', field_name):
                pass
            return
        import pdb; pdb.set_trace()

    def write_enum(self, out, field_name, field):
        # FIXME
        pass

    def write_array(self, out, field_name, field):
        self.arr_depth += 1
        
        self.putln(out, '.read = load_array,')
        self.putln(out, '.write = save_array,')
        self.putln(out, '.fresh = true,')
        self.putln(out, '.state = &json_array_state[{}],'.format(self.arr_depth))
        with self.write_params(out, 'load_array_params_t'):
            self.putln(
                out,
                '.array_len = sizeof_field({root_t}, {name}) / sizeof_field({root_t}, {name}[0]),'.format(
                    root_t=self.ROOT_TYPE,
                    name=field_name,
                ),
            )
            self.putln(
                out,
                '.item_size = sizeof_field({}, {}[0]),'.format(
                    self.ROOT_TYPE,
                    field_name,
                )
            )
            self.put_indented(out, '.item_handler = ')
            with self.indent(
                out,
                (
                    '((preset_section_handler_t) {',
                    '}),\n'
                )
            ):
                kind_writer = self.get_writer(field.type.kind)

            
        self.arr_depth -= 1
        
    def write_struct(self, out, field_name, field):
        self.write_object(out, field.type, field_name)
        
    @contextmanager
    def write_params(self, out, params_t, field_name=None):
        self.put_indented(out, '.params = &')
        with self.indent(
            out,
            (
                '(({}) {{'.format(params_t),
                '}),\n'
            )
        ):
            if field_name is not None:
                self.putln(
                    out,
                    '.dst_offset = offsetof({}, state.{}),'.format(
                        self.ROOT_TYPE,
                        field_name,
                    )
                )
                self.putln(
                    out,
                    '.dst_size = sizeof_field({}, state.{}),'.format(
                        self.ROOT_TYPE,
                        field_name,
                    )
                )
            yield

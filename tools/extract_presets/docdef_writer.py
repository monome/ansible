from collections import deque
from contextlib import contextmanager

from cffi import FFI


class DocdefWriter:
    INDENT_STR = '\t'
    FIRMWARE_NAME = 'ansible'
    ROOT_TYPE = 'nvram_data_t'

    def __init__(self, schemata, name, version):
        self.name = name
        self.indentation = 0
        self.obj_depth = 0
        self.max_obj_depth = 0
        self.arr_depth = 0
        self.max_arr_depth = 0
        self.state_path = deque()

        self.ffi = FFI()
        try:
            self.schema = schemata[version](self.ffi)
        except KeyError:
            raise NotImplementedError("don't know struct defs for version {}".format(version))
        else:
            self.ffi.cdef(self.schema.cdef())

        self.starting_type = self.ffi.typeof(self.ROOT_TYPE)

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
        self.putln(
            out,
            'preset_section_handler_t {} = '.format(self.name)
        )
        with self.indent(out, ('{', '};\n')):
            self.write_object(out, self.starting_type, True)
        print('load_object_state_t json_object_state[{}];'.format(self.max_obj_depth + 1))
        print('load_array_state_t json_array_state[{}];'.format(self.max_arr_depth + 1))

    def write_object(self, out, ctype, params_only=False):
        self.putln(out, '.read = load_object,')
        self.putln(out, '.write = save_object,')
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
                    with self.descend_object(field_name):
                        kind_writer = self.get_writer(field.type.kind)
                        self.put_indented(out, '')
                        with self.indent(out, ('{', '},\n')):
                            self.putln(out, '.name = "{}",'.format(field_name))
                            kind_writer(out, field_name, field.type)

    def get_writer(self, kind):
        return getattr(self, 'write_{}'.format(kind))

    def write_primitive(self, out, field_name, ctype):
        self.putln(out, '.read = load_scalar,')
        if ctype.cname.startswith('uint'):
            self.putln(out, '.write = save_number,')
            with self.write_params(out, 'load_scalar_params_t', True, True):
                pass
            return
        if ctype.cname.startswith('int'):
            self.putln(out, '.write = save_number,')
            with self.write_params(out, 'load_scalar_params_t', True, True):
                self.putln(out, '.signed_val = true,')
            return
        if ctype.cname == '_Bool':
            self.putln(out, '.write = save_bool,')
            with self.write_params(out, 'load_scalar_params_t', True):
                pass
            return
        import pdb; pdb.set_trace()

    def write_enum(self, out, field_name, ctype):
        self.putln(out, '.read = load_enum,'),
        self.putln(out, '.write = save_enum,'),
        with self.write_params(out, 'load_enum_params_t', True):
            self.put_indented(out, '.options = ')
            with self.indent(
                out,
                (
                    '((const char* []) {',
                    '}),\n',
                ),
            ):
                for i in range(len(ctype.elements)):
                    self.putln(out, '"{}",'.format(ctype.elements[i]))

    def write_array(self, out, field_name, ctype):
        if ctype.item.kind == 'primitive':
            self.write_buffer(out, field_name, ctype)
            return
        
        self.putln(out, '.read = load_array,')
        self.putln(out, '.write = save_array,')
        self.putln(out, '.fresh = true,')
        self.putln(out, '.state = &json_array_state[{}],'.format(self.arr_depth))
        with self.write_params(out, 'load_array_params_t'):
            self.putln(
                out,
                '.array_len = sizeof_field({root_t}, {name}) / sizeof_field({root_t}, {name}[0]),'.format(
                    root_t=self.ROOT_TYPE,
                    name=self.path,
                ),
            )
            self.putln(
                out,
                '.item_size = sizeof_field({}, {}[0]),'.format(
                    self.ROOT_TYPE,
                    self.path,
                )
            )

            with self.descend_array():
                self.put_indented(out, '.item_handler = &')
                with self.indent(
                    out,
                    (
                        '((preset_section_handler_t) {',
                        '}),\n'
                    )
                ):
                    kind_writer = self.get_writer(ctype.item.kind)
                    kind_writer(out, field_name, ctype.item)


    def write_buffer(self, out, field_name, ctype):
        self.putln(out, '.read = load_buffer,')
        self.putln(out, '.write = save_buffer,')
        self.putln(out, '.fresh = true,')
        self.putln(out, '.state = &load_buffer_state,')
        with self.write_params(out, 'load_buffer_params_t', True):
            pass
        
    def write_struct(self, out, field_name, ctype):
        self.write_object(out, ctype)
        
    @contextmanager
    def write_params(self, out, params_t, with_offset=False, with_size=False):
        self.put_indented(out, '.params = &')
        with self.indent(
            out,
            (
                '(({}) {{'.format(params_t),
                '}),\n'
            )
        ):
            if with_offset:
                self.putln(
                    out,
                    '.dst_offset = offsetof({}, {}),'.format(
                        self.ROOT_TYPE,
                        self.path,
                    )
                )
            if with_size:
                self.putln(
                    out,
                    '.dst_size = sizeof_field({}, {}),'.format(
                        self.ROOT_TYPE,
                        self.path,
                    )
                )
            yield

    @contextmanager
    def descend(self, s):
        self.state_path.append(s)
        yield
        self.state_path.pop()
            
    @contextmanager
    def descend_array(self):
        self.arr_depth += 1
        name = self.state_path.pop()
        with self.descend('{}[0]'.format(name)):
            yield
        self.state_path.append(name)
        if self.arr_depth > self.max_arr_depth:
            self.max_arr_depth = self.arr_depth
        self.arr_depth -= 1

    @contextmanager
    def descend_object(self, name):
        self.obj_depth += 1
        with self.descend(name):
            yield
        if self.obj_depth > self.max_obj_depth:
            self.max_obj_depth = self.obj_depth
        self.obj_depth -= 1
        
    @property
    def path(self):
        return '.'.join(self.state_path)
    

from abc import ABC, abstractmethod
import binascii
from collections import OrderedDict
from functools import reduce
from itertools import repeat
from socket import htons, htonl


class PresetSchema(ABC):
    def __init__(self, ffi):
        self.ffi = ffi

    @abstractmethod
    def check(self, nvram):
        pass

    @abstractmethod
    def firmware_name(self):
        pass

    def root_type(self):
        return 'nvram_data_t'

    @abstractmethod
    def app_list(self):
        pass

    @abstractmethod
    def cdef(self):
        pass

    @abstractmethod
    def meta(self, nvram):
        pass

    @abstractmethod
    def shared(self, nvram):
        pass

    def encode_buffer(self, cdata):
        return binascii.hexlify(
            bytes(self.ffi.buffer(cdata))
        ).decode().upper()

    def pair(self, state, name, lam=None):
        mu = lam or (lambda x: x)
        struct_name, *rest = name.split(':', 1)
        json_name = rest[0] if rest else struct_name
        try:
            field = dict(self.ffi.typeof(state).fields)[struct_name]
        except AttributeError:
            field = None
        val = getattr(state, struct_name)
        return (json_name, mu(val, field))

    def scalar_settings(self, state, names):
        return OrderedDict(
            self.pair(state, name, self.hton)
            for name in names
        )

    def hton(self, val, field):
        # scalars always get unpacked as native byte order
        # so by the time they're loaded the sign has been lost
        f = {
            'uint16_t': htons,
            'int16_t': lambda x: htons(x) - 2**16,
            'uint32_t': htonl,
            'int32_t': lambda x: htonl(x) - 2**32,
        }.get(field.type.cname, lambda x: x)
        return f(val)

    def lambda_settings(self, state, lambdas_names):
        return OrderedDict(
            self.pair(state, name, lambda x, f: l(x))
            for name, l in lambdas_names
        )

    def array_settings(self, state, item_lambdas_names):
        return OrderedDict(
            self.pair(state, name, lambda xs, f: list(map(l, xs)))
            for name, l in item_lambdas_names
        )

    def enum_settings(self, state, types_names):
        return OrderedDict(
            self.pair(state, name, lambda x, f: self.enum_value(t, x, default))
            for name, t, default in types_names
        )

    def enum_value(self, t, val, default):
        enum_t = self.ffi.typeof(t)
        try:
            return enum_t.elements[val]
        except KeyError:
            return default

    def array_1d_settings(self, state, names):
        return OrderedDict(
            self.pair(state, name, lambda x, f: self.encode_buffer(x))
            for name in names
        )

    def array_2d_settings(self, state, names):
        return self.array_settings(
            state,
            zip(names, repeat(self.encode_buffer)),
        )

    def combine(self, *dicts):
        return reduce(lambda l, r: l.update(r) or l, dicts)

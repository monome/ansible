from abc import ABC, abstractmethod
import binascii
from functools import reduce
from itertools import repeat


class PresetSchema(ABC):
    def __init__(self, ffi):
        self.ffi = ffi

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
        return binascii.hexlify(bytes(self.ffi.buffer(cdata))).decode()
    
    def scalar_settings(self, state, names):
        return {
            name: getattr(state, name)
            for name in names
        }
    
    def lambda_settings(self, state, lambdas_names):
        return {
            name: l(getattr(state, name))
            for name, l in lambdas_names
        }

    def array_settings(self, state, item_lambdas_names):
        return {
            name: [l(x) for x in getattr(state, name)]
            for name, l in item_lambdas_names
        }
    
    def enum_settings(self, state, types_names):
        return {
            name: self.enum_setting(t, getattr(state, name))
            for name, t in types_names
        }

    def enum_setting(self, t, val):
        enum_t = self.ffi.typeof(t)
        try:
            return enum_t.elements[val]
        except KeyError:
            return None
    
    def array_1d_settings(self, state, names):
        return {
            name: self.encode_buffer(getattr(state, name))
            for name in names
        }

    def array_2d_settings(self, state, names):
        return self.array_settings(
            state,
            zip(names, repeat(self.encode_buffer)),
        )
        # return {
        #     name: [self.encode_buffer(y) for y in getattr(state, name)]
        #     for name in names
        # }

    def combine(self, *dicts):
        return reduce(lambda l, r: l.update(r) or l, dicts)

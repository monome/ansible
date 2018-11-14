from abc import ABC, abstractmethod
import binascii
from functools import reduce


class PresetSchema(ABC):
    def __init__(self, ffi):
        self.ffi = ffi

    @abstractmethod
    def app_list(self):
        pass

    @abstractmethod
    def cdef(self):
        pass

    def encode_buffer(self, cdata):
        return binascii.hexlify(bytes(self.ffi.buffer(cdata))).decode()
    
    def scalar_settings(self, state, names):
        return {
            name: getattr(state, name)
            for name in names
        }

    def array_1d_settings(self, state, names):
        return {
            name: self.encode_buffer(getattr(state, name))
            for name in names
        }

    def array_2d_settings(self, state, names):
        return {
            name: [self.encode_buffer(y) for y in getattr(state, name)]
            for name in names
        }

    def combine(self, *dicts):
        return reduce(lambda l, r: l.update(r) or l, dicts)

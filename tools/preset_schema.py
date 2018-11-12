from abc import ABC, abstractmethod
from functools import reduce


class PresetSchema(ABC):
    @abstractmethod
    def app_list(self):
        pass

    @abstractmethod
    def cdef(self):
        pass
    
    def scalar_settings(self, state, names):
        return {
            name: getattr(state, name)
            for name in names
        }

    def array_1d_settings(self, state, names):
        return {
            name: [item for item in getattr(state, name)]
            for name in names
        }

    def array_2d_settings(self, state, names):
        return {
            name: [[x for x in y] for y in getattr(state, name)]
            for name in names
        }

    def combine(self, *dicts):
        return reduce(lambda l, r: l.update(r) or l, dicts)

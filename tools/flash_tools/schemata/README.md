# Adding a schema definition

The critical part of a schema definition class is the cdef() method,
which needs to contain all the structs needed to build your target
struct.  The Python CFFI library can handle typedefs and macros,
provided that macro definitions are constant numeric values - any type
aliasing macros will have to be rewritten as typedefs.

It's also important to be careful to get the packing right. In
particular CFFI will always assign sizeof(long) for enums, whereas
they're typically bytes for AVR32. You can change the enum fields to
have a type of uint8_t, but define the enums anyway so you can access
their string values. If you generate JSON document definitions for
structs with enum fields this does mean you might have to clean them
up by hand. It's good to check that self.ffi.sizeof('nvram_data_t')
matches the size of the flash_nvram section in your ***.sym file.

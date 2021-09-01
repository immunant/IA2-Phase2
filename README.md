# IA2 Phase 2

## Header Rewriter
The header rewriter is an out-of-tree clang tool based on LibTooling.
To run it, use the following command after building it:
```
$ /path/to/header-rewriter /path/to/source.h -o /path/to/wrapper_output_file.c -- -I/usr/lib/clang/A.B.C/include
```

The wrapper library can then be compiled with (assuming the original library is liboriginal.so):
```
$ clang /path/to/wrapper_output_file.c -shared -Wl,--version-script,/path/to/wrapper_output_file.c.syms -loriginal -o libwrapper.so
```

The user application can then link against libwrapper.so using the rewritten
header. For testing you will likely need to add `-Wl,-rpath=path/to/libs` so
that the linker and dynamic loader can find the original and wrapper libraries.

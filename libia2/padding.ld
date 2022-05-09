/*
  This linker script ensures that the start and end of the program headers in
  the default linker script for x86_64 are page-aligned when building
  executables. It assumes that the default loadable program headers start and
  end with the following sections:
    - .interp and .rela.plt
    - .init and .fini
    - .rodata and .eh_frame
    - .init_array and .bss

  Only INSERT commands are used to allow augmenting another linker script
  instead of overriding it. It also may work with explicitly specified linker
  scripts, but it is the user's responsibility to ensure that any new loadable
  segments have their start and end page-aligned.
*/
SECTIONS {
    .rela.plt_padding : {
        . = ALIGN(4096);
    }
} INSERT AFTER .rela.plt;

SECTIONS { . = ALIGN(4096); } INSERT BEFORE .init;
SECTIONS {
    .fini_padding : {
        . = ALIGN(4096);
    } =0xcc
} INSERT AFTER .fini;

SECTIONS { . = ALIGN(4096); } INSERT BEFORE .rodata;
SECTIONS {
    .eh_frame_padding : {
        . = ALIGN(4096);
    }
} INSERT AFTER .eh_frame;

SECTIONS {
    . = ALIGN(4096);
    __start_dynamic = .;
} INSERT BEFORE .dynamic;

SECTIONS {
    .dynamic_padding : {
        . = ALIGN(4096);
        __stop_dynamic_padding = .;
    }
} INSERT AFTER .dynamic;

SECTIONS {
    . = ALIGN(4096);
    __start_gnu_hash = .;
} INSERT BEFORE .gnu.hash;

SECTIONS {
    .gnu.hash_padding : {
        . = ALIGN(4096);
        __stop_gnu_hash_padding = .;
    }
} INSERT AFTER .gnu.hash;

SECTIONS {
    . = ALIGN(4096);
    __start_dynsym = .;
} INSERT BEFORE .dynsym;

SECTIONS {
    .dynsym_padding : {
        . = ALIGN(4096);
        __stop_dynsym_padding = .;
    }
} INSERT AFTER .dynsym;

SECTIONS {
    . = ALIGN(4096);
    __start_dynstr = .;
} INSERT BEFORE .dynstr;

SECTIONS {
    .dynstr_padding : {
        . = ALIGN(4096);
        __stop_dynstr_padding = .;
    }
} INSERT AFTER .dynstr;

SECTIONS {
    . = ALIGN(4096);
    __start_gnu_version = .;
} INSERT BEFORE .gnu.version;

SECTIONS {
    .gnu_version_padding : {
        . = ALIGN(4096);
        __stop_gnu_version_padding = .;
    }
} INSERT AFTER .gnu.version;

SECTIONS {
    . = ALIGN(4096);
    __start_interp = .;
} INSERT BEFORE .interp;

SECTIONS {
    .interp_padding : {
        . = ALIGN(4096);
        __stop_interp_padding = .;
    }
} INSERT AFTER .interp;

SECTIONS { . = ALIGN(4096); } INSERT BEFORE .init_array;
SECTIONS {
    .rodata_padding : {
        . = ALIGN(4096);
    }

    ia2_shared_rodata : {
        __start_ia2_shared_rodata = .;
        *(ia2_shared_rodata)
        . = ALIGN(4096);
        __stop_ia2_shared_rodata = .;
    }
} INSERT AFTER .rodata;
SECTIONS {
    .bss_padding : {
        . = ALIGN(4096);
    }
    /*
      The ALIGN in .bss_padding ensures ia2_shared_data is aligned and the next
      ALIGN ensures it is padded to a multiple of a page. In no input files
      contain ia2_shared_data sections this section will not be created in the
      output and the following ALIGN will have no effect.
    */
    ia2_shared_data : {
        __start_ia2_shared_data = .;
        *(ia2_shared_data)
        environ = .;
        . = ALIGN(4096);
        __stop_ia2_shared_data = .;
    }
} INSERT AFTER .bss;

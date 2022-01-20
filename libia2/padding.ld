/*
  This linker script ensures that the start and end of the program headers in
  the default linker script for x86_64 are page-aligned. It assumes that the
  default loadable program headers start and end with the following sections:
    - .interp and .rela.plt
    - .init and .fini
    - .rodata and .eh_frame
    - .init_array and .bss

  Only INSERT commands are used to allow augmenting another linker script
  instead of overriding it. It also may work with explicitly specified linker
  scripts, but it is the user's responsibility to ensure that any new loadable
  segments have their start and end page-aligned.
*/
SECTIONS { . = ALIGN(4096); } INSERT BEFORE .interp;
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

SECTIONS { . = ALIGN(4096); } INSERT BEFORE .init_array;
SECTIONS {
    .bss_padding : {
        . = ALIGN(4096);
    }
    ia2_shared_data : {
        *(ia2_shared_data)
        . = ALIGN(4096);
    }
} INSERT AFTER .bss;

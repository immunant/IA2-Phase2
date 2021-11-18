/*
  This linker script only uses INSERT commands to allow augmenting either
  the system's default linker script or one provided by the user. This assumes
  that the default loadable program headers start and end with the following
  sections:
    - .interp and .rela.plt
    - .init and .fini
    - .rodata and .eh_frame
    - .init_array and .bss
  TODO: Users interested in using linker scripts can ensure that program headers
  are page-aligned themselves, but it'd be good to see what common command-line
  flags modify sections that can break this script.
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
} INSERT AFTER .bss;

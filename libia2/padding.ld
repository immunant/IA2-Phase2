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

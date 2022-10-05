/*
  Only INSERT [ AFTER | BEFORE ] commands are used to allow augmenting another
  linker script instead of overriding it. It also may work with explicitly
  specified linker scripts, but it is the user's responsibility to ensure that
  any new loadable segments have their start and end page-aligned.
*/
/*
   These SECTIONS commands place __start_$SECTION at the page boundary following
   the start of $SECTION, but the input sections are all placed after
   __start_$SECTION and before __stop_$SECTION.
*/
SECTIONS {
    ia2_shared_rodata : {
        . = ALIGN(4096);
        __start_ia2_shared_rodata = .;
        *(ia2_shared_rodata)
        *ngx_string.o(.rodata)
        . = ALIGN(4096);
        __stop_ia2_shared_rodata = .;
    }
} INSERT AFTER .rodata;

SECTIONS {
    ia2_shared_data : {
        . = ALIGN(4096);
        __start_ia2_shared_data = .;
        *(ia2_shared_data)
        . = ALIGN(4096);
        __stop_ia2_shared_data = .;
    }
} INSERT AFTER .data;

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
    __start_bss = .;
} INSERT BEFORE .bss;
            
SECTIONS {
    .bss_padding : {
        . = ALIGN(4096);
        __stop_bss_padding = .;
    }
} INSERT AFTER .bss;

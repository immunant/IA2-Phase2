/*
  This linker script ensures that the start and end of the program headers in
  the default linker script for x86_64 are page-aligned when building
  executables. It assumes that the default loadable segments start and end with
  the following sections:
    for ld.bfd:
    - .interp and .rela.plt
    - .init and .fini
    - .rodata and .eh_frame
    - .init_array and .bss
    for ld.lld:
    - .interp and .eh_frame
    - .text and .plt
    - .fini_array and .got.plt
    - .data and .bss

  This can be verified by looking at the output of `readelf --segments`: each
  loadable segment should start with the first section mentioned in the pairs
  above and end with the second followed by a ".padding" section.

  The padding sections fill out each segment so that it ends at a page-aligned
  address, and the anonymous sections (for some toolchains) are necessary to
  push back the first sections in a segment so they start at page-aligned
  addresses.

  Only INSERT commands are used to allow augmenting another linker script
  instead of overriding it. It also may work with explicitly specified linker
  scripts, but it is the user's responsibility to ensure that any new loadable
  segments have their start and end page-aligned.
*/
SECTIONS { . = ALIGN(4096); } INSERT BEFORE .interp;
SECTIONS {
    .plt_padding : {
        . = ALIGN(4096);
    }
} INSERT AFTER .plt;

/*
  This section cannot be called .rela.plt_padding because starting a section
  name with .rela has special semantics for ld.lld
*/
SECTIONS {
    ._rela.plt_padding : {
        . = ALIGN(4096);
    }
} INSERT AFTER .rela.plt;

SECTIONS { . = ALIGN(4096); } INSERT BEFORE .init;

/*
  We'd like to name this section .got.plt.padding, but we need to name it .jcr
  so that it's created as a RELRO and gets assigned the same segment as
  .got.plt. Similarly, it would make more sense to say `AFTER .got.plt` than
  `BEFORE .data`, but gcc does not always emit a .got.plt section, and we have
  no conditionals in linker scripts. The .data section happens to come next, so
  we just say `BEFORE` that and let the attributes cause this section to be in
  the same segment as .got.plt
*/
SECTIONS {
    .jcr : {
        . = ALIGN(4096);
        *(.jcr)
    }
} INSERT BEFORE .data;

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
    /*
      This section must be NOLOAD in order to match the flags that ld.lld gives
      to .bss (ALLOC only).
    */
    .bss_padding (NOLOAD) : {
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
        . = ALIGN(4096);
        __stop_ia2_shared_data = .;
    }
} INSERT AFTER .bss;

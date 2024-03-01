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
    ia2_shared_data ALIGN(4096): {
        __start_ia2_shared_data = .;
        *(ia2_shared_data)
        /*
           This weird alignment expression is required so that the section will
           get elided if empty. ld.bfd special-cases this syntax to mean this,
           which is not documented but appears in its default linker script; see
           binutils commit 9d12f64cdc6540b75938097d7f4ab460bb346528
        */
        . = ALIGN(. != 0 ? 4096 : 1);
        __stop_ia2_shared_data = .;
    }
} INSERT AFTER .data;

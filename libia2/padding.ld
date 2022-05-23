/*
  For binaries: first, pad the .interp section, then defer to shared_lib.ld
*/
SECTIONS { . = ALIGN(4096); } INSERT BEFORE .interp;
INCLUDE shared_lib.ld


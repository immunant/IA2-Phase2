#pragma once

#define LOCAL static

typedef void (*fn_ptr_ty)(void);

static void inline_noop(void) {
  printf("called %s defined in header\n", __func__);
}

fn_ptr_ty *get_ptrs_in_main(void);
fn_ptr_ty *get_ptrs_in_lib(void);

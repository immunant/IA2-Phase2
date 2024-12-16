#pragma once

struct in_memory {
  char arr[32];
};

// This function does nothing
void foo(void);

// This returns an integer 1
int return_val(void);

// This takes an integer, expects value 1
void arg1(int x);

// Expects x==1 and y==2
void arg2(int x, int y);

// Expects x==1 and y==2
void arg3(int x, float f, int y);

void many_args(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j);

void arg_in_memory(struct in_memory im);

struct in_memory ret_in_memory(int x);

typedef struct in_memory(*fn_ptr_ret_in_mem)(int);
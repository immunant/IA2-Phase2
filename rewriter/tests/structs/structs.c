/*
RUN: cat structs_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include "structs.h"
#include <stdio.h>

// LINKARGS: --wrap=cksum_s1
int cksum_s1(struct s1 s) {
	return s.i1 + s.i2;
}

// LINKARGS: --wrap=cksum_s10
__int128 cksum_s10(struct s10 s) {
	return s.i1 + s.i2 + s.i3 + s.z1;
}

// LINKARGS: --wrap=cksum_s11
unsigned int cksum_s11(struct s11 s) {
	return 9 * s.az1[0]
		+ 8 * s.az1[1]
		+ 7 * s.az1[2]
		+ 6 * s.az1[3]
		+ 5 * s.az1[4]
		+ 4 * s.az1[5]
		+ 3 * s.az1[6]
		+ 2 * s.az1[7]
		+ 1 * s.az1[8];
}

// LINKARGS: --wrap=cksum_s12
unsigned int cksum_s12(struct s12 s) {
	return 9 * s.ac1[0]
		+ 8 * s.ac1[1]
		+ 7 * s.ac1[2]
		+ 6 * s.ac1[3]
		+ 5 * s.ac1[4]
		+ 4 * s.ac1[5]
		+ 3 * s.ac1[6]
		+ 2 * s.ac1[7]
		+ 1 * s.ac1[8];
}

// LINKARGS: --wrap=cksum_s13
unsigned int cksum_s13(struct s13 s) {
    unsigned int x0 = s.x & 0xffffffff;
    unsigned int x1 = (s.x >> 32) & 0xffffffff;
    unsigned int x2 = (s.x >> 64) & 0xffffffff;
    unsigned int x3 = (s.x >> 96) & 0xffffffff;
    return x0 + x1 + x2 + x3;
}

// LINKARGS: --wrap=cksum_s14
unsigned int cksum_s14(struct s14 s) {
	return s.u1 + s.u32 + s.field2 + s.field3 + s.field7;
}

// LINKARGS: --wrap=cksum_s3
int cksum_s3(struct s3 s) {
	return s.i1 + s.c1 + s.u1;
}

// LINKARGS: --wrap=cksum_s4
float cksum_s4(struct s4 s) {
	return s.f1 + s.f2;
}

// LINKARGS: --wrap=cksum_s5
float cksum_s5_f(struct s5 s) {
	return s.f1;
}

// LINKARGS: --wrap=cksum_s5
size_t cksum_s5_z(struct s5 s) {
	return s.i1 + (size_t)s.p1 + s.ac1[0] + s.ac1[1] + s.ac1[2] + (size_t)s.p2;
}

// LINKARGS: --wrap=cksum_s6
size_t cksum_s6(struct s6 s) {
	return s.i1 + s.c1 + s.i2 + s.c2 + s.z1;
}

// LINKARGS: --wrap=cksum_s7
unsigned int cksum_s7(struct s7 s) {
	return s.b1 + s.c1 + s.u1;
}

// LINKARGS: --wrap=cksum_s8
size_t cksum_s8(struct s8 s) {
	return s.b1 + s.c1 + s.z1 + s.c2 + s.z2 + s.z3;
}

// LINKARGS: --wrap=cksum_s9
__int128 cksum_s9(struct s9 s) {
	return s.i1 + s.i2 + s.i3;
}

// LINKARGS: --wrap=extract_s2
float extract_s2(struct s2 s) {
	return s.f1;
}

// LINKARGS: --wrap=get_s1
struct s1 get_s1(void) {
	struct s1 s = {
		3,
		96,
	};
	return s;
}

// LINKARGS: --wrap=get_s10
struct s10 get_s10(void) {
	struct s10 s = {
		.i1 = 96506328,
		.i2 = 1846,
		.i3 = 3254,
		.z1 = 6853785,
	};
	return s;
}

// LINKARGS: --wrap=get_s11
struct s11 get_s11(void) {
	struct s11 s = {
		.az1 = {1, 2, 3, 4, 5, 6, 7, 8, 9},
	};
	return s;
}

// LINKARGS: --wrap=get_s12
struct s12 get_s12(void) {
	struct s12 s = {
		.ac1 = {1, 2, 3, 4, 5, 6, 7, 8, 9},
	};
	return s;
}

// LINKARGS: --wrap=get_s13
struct s13 get_s13(void) {
    struct s13 s = {
        .x = 0x7fffeeeeddddcccc
    };
    s.x <<= 64;
    s.x |= 0xbbbbaaaa99998888;
    return s;
}

// LINKARGS: --wrap=get_s14
struct s14 get_s14(void) {
	struct s14 s = {
		0,
		1,
		0,
		0,
		1,
		0,
		0,
		0,
		1,
		0,
		0,
		0,
		0,
		1,
		0,
		0,
		0,
		0,
		0,
		1,
		0,
		0,
		0,
		0,
		0,
		1,
		0,
		1,
		1,
		1,
		1,
		0,
		.field2 = 20000,
		.field3 = 30000,
		.field4 = 40000,
		.field5 = 50000,
		.field6 = 60000,
		.field7 = 70000,
		.field8 = 80000,
	};
	return s;
}

// LINKARGS: --wrap=get_s2
struct s2 get_s2(void) {
	struct s2 s = {
		3.14,
	};
	return s;
}

// LINKARGS: --wrap=get_s3
struct s3 get_s3(void) {
	struct s3 s = {
		.i1 = 50,
		.c1 = 65,
		.u1 = 65379,
	};
	return s;
}

// LINKARGS: --wrap=get_s4
struct s4 get_s4(void) {
	struct s4 s = {
		.f1 = 3.14,
		.f2 = 0.0016,
	};
	return s;
}

// LINKARGS: --wrap=get_s5
struct s5 get_s5(void) {
	struct s5 s = {
		.i1 = 31,
		.p1 = (void*)0x4000030,
		.ac1 = {1, 2, 3},
		.p2 = (void*)0xffffbeef,
		.f1 = 98.7,
	};
	return s;
}

// LINKARGS: --wrap=get_s5_int
struct s5 get_s5_int(int i1) {
	struct s5 s = {
		.i1 = i1,
		.p1 = (void*)0x4000030,
		.ac1 = {1, 2, 3},
		.p2 = (void*)0xffffbeef,
		.f1 = 98.7,
	};
	return s;
}

// LINKARGS: --wrap=get_s6
struct s6 get_s6(void) {
	struct s6 s = {
		.i1 = 8043,
		.c1 = 4,
		.i2 = 695465,
		.c2 = 100,
		.z1 = 0x9400000,
	};
	return s;
}

// LINKARGS: --wrap=get_s7
struct s7 get_s7(void) {
	struct s7 s = {
		.b1 = true,
		.c1 = 55,
		.u1 = 454000,
	};
	return s;
}

// LINKARGS: --wrap=get_s8
struct s8 get_s8(void) {
	struct s8 s = {
		.b1 = true,
		.c1 = 15,
		.z1 = 454000,
		.c2 = 13,
		.z2 = 9685031,
		.z3 = 62121255,
	};
	return s;
}

// LINKARGS: --wrap=get_s9
struct s9 get_s9(void) {
	struct s9 s = {
		.i1 = 96506328,
		.i2 = 1846,
		.i3 = 3254,
	};
	return s;
}

// LINKARGS: --wrap=inc_s3
struct s3 inc_s3(struct s3 s) {
	s.i1++;
	s.c1++;
	s.u1++;
	return s;
}

// LINKARGS: --wrap=inc_s6
struct s6 inc_s6(struct s6 s) {
	s.i1++;
	s.c1++;
	s.i2++;
	s.c2++;
	s.z1++;
	return s;
}

// LINKARGS: --wrap=mix_s6
struct s6 mix_s6(struct s6 s1, struct s6 s2) {
	s1.i1 += s2.i1 / 2;
	s1.c1 += s2.c1 / 2;
	s1.i2 += s2.i2 / 2;
	s1.c2 += s2.c2 / 2;
	s1.z1 += s2.z1 / 2;
	return s1;
}

// LINKARGS: --wrap=print_s5
void print_s5(struct s5 s) {
	printf("s5 { %d %p [%d,%d,%d] %p %.4f }\n",
		s.i1, s.p1, s.ac1[0], s.ac1[1], s.ac1[2], s.p2, s.f1);
}


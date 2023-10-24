/*
*/
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include "structs.h"
#include <ia2.h>
#include <math.h>

/*
    This program tests that a trusted binary can pass and return structs of various shapes
    and sizes from an untrusted shared library.
*/

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

#define EPSILON 0.0001f

#define check_close_float(name, val) { cr_assert_lt(fabs(name(s) - (val)), EPSILON); }
#define check_field_float(name, val) { cr_assert_lt(fabs(s.name - (val)), EPSILON); }

#define check_eq_int(name, val) { cr_assert(name(s) == val); }
#define check_field_int(name, val) { cr_assert(s.name == val); }

#define check_eq_size(name, val) { cr_assert(name(s) == (size_t)val); }
#define check_field_size(name, val) { cr_assert(s.name == (size_t)val); }

#define check_field_ptr(name, val) { cr_assert(s.name == (void*)val); }

#define check_eq_i128(name, val) { cr_assert(name(s) == (__int128)val); }
#define check_field_i128(name, val) { cr_assert(s.name == (__int128)val); }

Test(structs, main) {
	/* For each struct, test passing it to functions, returning it from functions
	(see structs.c), and calls with various combinations of argument/return types. */
	{
		struct s1 s = {
			.i1 = 90,
			.i2 = 89,
		};
		check_eq_int(cksum_s1, s.i1 + s.i2);
		s = get_s1();
		check_field_int(i1, 3);
		check_field_int(i2, 96);
	}

	{
		struct s2 s = {
			.f1 = 999.99,
		};
		check_close_float(extract_s2, s.f1);
		cr_assert(extract_s2(s) == s.f1);
		cr_assert(fabs(extract_s2(s) - 999.99) < EPSILON);
		s = get_s2();
		cr_assert(fabs(s.f1 - 3.14) < EPSILON);
	}

	{
		struct s3 s = {
			.i1 = 400,
			.c1 = 5,
			.u1 = 5071,
		};
		cr_assert(cksum_s3(s) == 400 + 5 + 5071);
		s = get_s3();
		check_field_int(i1, 50);
		check_field_int(c1, 65);
		check_field_int(u1, 65379);
	}

	{
		struct s4 s = {
			.f1 = 400.5,
			.f2 = 52480.75,
		};
		check_close_float(cksum_s4, 400.5 + 52480.75);
		s = get_s4();
		check_field_float(f1, 3.14);
		check_field_float(f2, 0.0016);
	}

	{
		struct s5 s = {
			.i1 = 6976,
			.p1 = (void*)0xb17ebee7,
			.ac1 = {78, 45, 32},
			.p2 = (void*)0xba5edba5edba5ed,
			.f1 = 390.56,
		};
		check_s5(s);
		check_close_float(cksum_s5_f, 390.56);
		check_eq_size(cksum_s5_z, 6976 + 0xb17ebee7 + 78 + 45 + 32 + 0xba5edba5edba5ed);
		s = get_s5();
		check_field_int(i1, 31);
		check_field_ptr(p1, 0x4000030);
		check_field_int(ac1[0], 1);
		check_field_int(ac1[1], 2);
		check_field_int(ac1[2], 3);
		check_field_ptr(p2, 0xffffbeef);
		check_field_float(f1, 98.7);

		s = get_s5_int(99954);
		check_field_int(i1, 99954);
	}

	{
		struct s6 s = {
			.i1 = 100,
			.c1 = 12,
			.i2 = 256,
			.c2 = 7,
			.z1 = 999999,
		};
		check_eq_size(cksum_s6, 100 + 12 + 256 + 7 + 999999);
		s = inc_s6(s);
		check_field_int(i1, 101);
		check_eq_size(cksum_s6, 100 + 12 + 256 + 7 + 999999 + 5);
		struct s6 s_orig = s;

		s = get_s6();
		check_field_int(i1, 8043);
		check_field_int(c1, 4);
		check_field_int(i2, 695465);
		check_field_int(c2, 100);
		check_field_size(z1, 0x9400000);

		s = mix_s6(s_orig, s);
		check_field_int(i1, 1 + 100 + 8043 / 2);
		check_field_int(c1, 1 + 12 + 4 / 2);
		check_field_int(i2, 1 + 256 + 695465 / 2);
		check_field_int(c2, 1 + 7 + 100 / 2);
		check_field_size(z1, 1 + 999999 + 0x9400000 / 2);
	}

	{
		struct s7 s = {
			.b1 = false,
			.c1 = 40,
			.u1 = 4034019,
		};
		check_eq_int(cksum_s7, 0 + 40 + 4034019);
		s = get_s7();
		check_field_int(b1, true);
		check_field_int(c1, 55);
		check_field_int(u1, 454000);
	}

	{
		struct s8 s = {
			.b1 = true,
			.c1 = 10,
			.z1 = 9954,
			.c2 = 20,
			.z2 = 14400,
			.z3 = 999333111,
		};
		check_eq_size(cksum_s8, 1 + 10 + 9954 + 20 + 14400 + 999333111);
		s = get_s8();
		check_field_int(b1, true);
		check_field_int(c1, 15);
		check_field_size(z1, 454000);
		check_field_int(c2, 13);
		check_field_size(z2, 9685031);
		check_field_size(z3, 62121255);
	}

	{
		struct s9 s = {
			.i1 = 2002,
			.i2 = 549,
			.i3 = 1500,
		};
		check_eq_i128(cksum_s9, 2002 + 549 + 1500);
		s = get_s9();
		check_field_int(i1, 96506328);
		check_field_int(i2, 1846);
		check_field_int(i3, 3254);
	}

	{
		struct s10 s = {
			.i1 = 40504030,
			.i2 = 540,
			.i3 = 1240,
			.z1 = 100500,
		};
		check_eq_i128(cksum_s10, 40504030 + 540 + 1240 + 100500);
		s = get_s10();
		check_field_i128(i1, 96506328);
		check_field_i128(i2, 1846);
		check_field_i128(i3, 3254);
		check_field_size(z1, 6853785);
	}

	{
		struct s11 s = {
			.az1 = {'a', 'b', 'c', 4, 5, 5, 5, 1, 'f'},
		};
        //print_s11(s);
		check_eq_int(cksum_s11,
			9 * 'a' +
			8 * 'b' +
			7 * 'c' +
			6 * 4 +
			5 * 5 +
			4 * 5 +
			3 * 5 +
			2 * 1 +
			1 * 'f');
		s = get_s11();
		check_field_int(az1[0], 1);
		check_field_int(az1[1], 2);
		check_field_int(az1[2], 3);
		check_field_int(az1[3], 4);
		check_field_int(az1[4], 5);
		check_field_int(az1[5], 6);
		check_field_int(az1[6], 7);
		check_field_int(az1[7], 8);
		check_field_int(az1[8], 9);
	}

	{
		struct s12 s = {
			.ac1 = {'a', 'b', 'c', 4, 5, 5, 5, 1, 'f'},
		};
		check_eq_int(cksum_s12,
			9 * 'a' +
			8 * 'b' +
			7 * 'c' +
			6 * 4 +
			5 * 5 +
			4 * 5 +
			3 * 5 +
			2 * 1 +
			1 * 'f');
		s = get_s12();
		check_field_int(ac1[0], 1);
		check_field_int(ac1[1], 2);
		check_field_int(ac1[2], 3);
		check_field_int(ac1[3], 4);
		check_field_int(ac1[4], 5);
		check_field_int(ac1[5], 6);
		check_field_int(ac1[6], 7);
		check_field_int(ac1[7], 8);
		check_field_int(ac1[8], 9);
	}

    {
        struct s13 s = {
            .x = 0x7fffeeeeddddcccc
        };
        s.x <<= 64;
        s.x |= 0xbbbbaaaa99998888;
        check_eq_int(cksum_s13,
            (0x99998888 & 0xffffffff) +
            (0xbbbbaaaa & 0xffffffff) +
            (0xddddcccc & 0xffffffff) +
            (0x7fffeeee & 0xffffffff));
        s = get_s13();
        check_field_i128(x, (((__int128)0x7fffeeeeddddcccc << 64) | 0xbbbbaaaa99998888));
    }

	{
		struct s14 s = { 0 };
		s.u1 = 1;
		s.u32 = 1;
		check_eq_int(cksum_s14, 2);
		s = get_s14();
		check_field_int(u1, 0);
		check_field_int(u2, 1);
		check_field_int(u3, 0);
		check_field_int(u4, 0);
		check_field_int(u5, 1);
		check_field_int(u6, 0);
		check_field_int(u7, 0);
		check_field_int(u8, 0);
		check_field_int(u9, 1);
		check_field_int(u10, 0);
		check_field_int(u11, 0);
		check_field_int(u12, 0);
		check_field_int(u13, 0);
		check_field_int(u14, 1);
		check_field_int(u15, 0);
		check_field_int(u16, 0);
		check_field_int(u17, 0);
		check_field_int(u18, 0);
		check_field_int(u19, 0);
		check_field_int(u20, 1);
		check_field_int(u21, 0);
		check_field_int(u22, 0);
		check_field_int(u23, 0);
		check_field_int(u24, 0);
		check_field_int(u25, 0);
		check_field_int(u26, 1);
		check_field_int(u27, 0);
		check_field_int(u28, 1);
		check_field_int(u29, 1);
		check_field_int(u30, 1);
		check_field_int(u31, 1);
		check_field_int(u32, 0);
		check_field_int(field2, 20000);
		check_field_int(field3, 30000);
		check_field_int(field4, 40000);
		check_field_int(field5, 50000);
		check_field_int(field6, 60000);
		check_field_int(field7, 70000);
		check_field_int(field8, 80000);
	}
}

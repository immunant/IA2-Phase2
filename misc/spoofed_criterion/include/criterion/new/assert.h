#include <assert.h>

#define cr_assert assert
#define cr_fatal assert


#define cr_assert_lt(a, b) assert((a) < (b))
#define cr_assert_eq(a, b) assert((a) == (b))
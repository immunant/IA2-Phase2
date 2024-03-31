#include <assert.h>

#define cr_assert assert
#define cr_assert_eq(a,b) cr_assert((a) == (b))
#define cr_fatal assert

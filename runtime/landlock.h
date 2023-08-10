#pragma once

#define _GNU_SOURCE
#include <linux/landlock.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

static inline int
landlock_create_ruleset(const struct landlock_ruleset_attr *const attr,
                        const size_t size, const __u32 flags) {
  return syscall(__NR_landlock_create_ruleset, attr, size, flags);
}

static inline int landlock_add_rule(const int ruleset_fd,
                                    const enum landlock_rule_type rule_type,
                                    const void *const rule_attr,
                                    const __u32 flags) {
  return syscall(__NR_landlock_add_rule, ruleset_fd, rule_type, rule_attr,
                 flags);
}

static inline int landlock_restrict_self(const int ruleset_fd,
                                         const __u32 flags) {
  return syscall(__NR_landlock_restrict_self, ruleset_fd, flags);
}

/* clang-format off */

#define ACCESS_FS_WITHIN ( \
  LANDLOCK_ACCESS_FS_READ_DIR | \
	LANDLOCK_ACCESS_FS_REMOVE_DIR | \
	LANDLOCK_ACCESS_FS_REMOVE_FILE | \
	LANDLOCK_ACCESS_FS_MAKE_CHAR | \
	LANDLOCK_ACCESS_FS_MAKE_DIR | \
	LANDLOCK_ACCESS_FS_MAKE_REG | \
	LANDLOCK_ACCESS_FS_MAKE_SOCK | \
	LANDLOCK_ACCESS_FS_MAKE_FIFO | \
	LANDLOCK_ACCESS_FS_MAKE_BLOCK | \
	LANDLOCK_ACCESS_FS_MAKE_SYM)

#define ACCESS_FS_ROUGHLY_READ ( \
	LANDLOCK_ACCESS_FS_EXECUTE | \
	LANDLOCK_ACCESS_FS_READ_FILE | \
	LANDLOCK_ACCESS_FS_READ_DIR)

#ifdef LANDLOCK_ACCESS_FS_REFER
#define LANDLOCK_ACCESS_FS_REFER_OR_0 LANDLOCK_ACCESS_FS_REFER
#else
#define LANDLOCK_ACCESS_FS_REFER_OR_0 0
#endif

#ifdef LANDLOCK_ACCESS_FS_TRUNCATE
#define LANDLOCK_ACCESS_FS_TRUNCATE_OR_0 LANDLOCK_ACCESS_FS_TRUNCATE
#else
#define LANDLOCK_ACCESS_FS_TRUNCATE_OR_0 0
#endif

#define ACCESS_FS_ROUGHLY_WRITE ( \
	LANDLOCK_ACCESS_FS_WRITE_FILE | \
	LANDLOCK_ACCESS_FS_REMOVE_DIR | \
	LANDLOCK_ACCESS_FS_REMOVE_FILE | \
	LANDLOCK_ACCESS_FS_MAKE_CHAR | \
	LANDLOCK_ACCESS_FS_MAKE_DIR | \
	LANDLOCK_ACCESS_FS_MAKE_REG | \
	LANDLOCK_ACCESS_FS_MAKE_SOCK | \
	LANDLOCK_ACCESS_FS_MAKE_FIFO | \
	LANDLOCK_ACCESS_FS_MAKE_BLOCK | \
	LANDLOCK_ACCESS_FS_MAKE_SYM | \
	LANDLOCK_ACCESS_FS_REFER_OR_0 | \
	LANDLOCK_ACCESS_FS_TRUNCATE_OR_0)

#define ACCESS_FILE ( \
	LANDLOCK_ACCESS_FS_EXECUTE | \
	LANDLOCK_ACCESS_FS_WRITE_FILE | \
	LANDLOCK_ACCESS_FS_READ_FILE | \
	LANDLOCK_ACCESS_FS_TRUNCATE_OR_0)

/* clang-format on */

#define LANDLOCK_ABI_LAST 3

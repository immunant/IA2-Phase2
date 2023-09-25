#pragma once
#include <linux/landlock.h>

/* enough context to know how to apply landlock filters to the process */
struct landlock_ctx {
  struct landlock_ruleset_attr ruleset_attr;
};

/* set up landlock context. returns nonzero on failure. */
int landlock_setup(struct landlock_ctx *ctx);

/* apply landlock context to the process. after this call, landlock cannot be
set up again and PR_SET_NO_NEW_PRIVS has been applied. returns nonzero on
failure. */
int landlock_apply_path_blacklist(const struct landlock_ctx *ctx, const char **paths);

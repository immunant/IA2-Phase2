#define _GNU_SOURCE
#include <ftw.h>
#include <linux/landlock.h>
#include <linux/prctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "forbid_paths.h"
#include "landlock_syscall.h"
#include "strv.h"

static int allow_path(const char *path, const int ruleset_fd, bool shallow) {
  struct landlock_path_beneath_attr path_beneath = {
      .parent_fd = -1,
  };

  struct stat statbuf;

  path_beneath.parent_fd = open(path, O_PATH | O_CLOEXEC);
  if (path_beneath.parent_fd < 0) {
    fprintf(stderr, "Failed to open \"%s\": %s\n", path, strerror(errno));
    return -1;
  }
  if (fstat(path_beneath.parent_fd, &statbuf)) {
    close(path_beneath.parent_fd);
    return -1;
  }
  path_beneath.allowed_access =
      ACCESS_FS_ROUGHLY_READ | ACCESS_FS_ROUGHLY_WRITE;

  /* limit non-directory files to access flags relevant for regular files */
  if (!S_ISDIR(statbuf.st_mode)) {
    path_beneath.allowed_access &= ACCESS_FILE;
  } else {
    /* if we only shallowly allow this directory, only allow READ_DIR */
    if (shallow)
      path_beneath.allowed_access = LANDLOCK_ACCESS_FS_READ_DIR;
  }

  /* add rule */
  if (landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &path_beneath,
                        0)) {
    fprintf(stderr, "Failed to update the ruleset with \"%s\": %s\n", path,
            strerror(errno));
    close(path_beneath.parent_fd);
    return -1;
  }
  close(path_beneath.parent_fd);

  return 0;
}

/* does path have the given prefix as a prefix of its path segments? */
static bool path_has_segment_prefix(const char *path, const char *prefix) {
  if (!strcmp(prefix, "/")) {
    return true;
  }

  while (*path == *prefix && *path && *prefix) {
    path++;
    prefix++;
  }

  if (*prefix == '\0' && (*path == '/' || *path == '\0')) {
    return true;
  }
  return false;
}

/*
Forbid access to the given paths, a null-terminated string vector.

Because landlock is an allowlist-based system, we need to allow everything other
than the specified paths. This is done by walking the directory tree downward
from / toward each path. At each level, directories that do not (transitively)
contain any forbidden files are recursively allowed, and directories that do are
shallowly allowed.

When the forbidden path is reached, its siblings are allowed by the same process
(which thereby avoids allowing other forbidden files).
*/

static struct forbid_ctx {
  const char **forbidden_paths;
  int ruleset_fd;
  int error;
} ctx;

/* At each level, directories that do not (transitively) contain any forbidden
files are recursively allowed, and directories that do are shallowly allowed. */
static int forbid_deep_or_shallow(const char *fpath, const struct stat *sb,
                                  int typeflag, struct FTW *ftwbuf) {
  const char *forbidden_path;

  bool contains_forbidden_path = false;
  bool is_forbidden_path = false;
  /* is any forbidden path a descendant of this dir? */
  for (int i = 0; (forbidden_path = ctx.forbidden_paths[i]); i++) {
    /* is forbidden_path a descendant of this dir? */
    bool prefix_matches = path_has_segment_prefix(forbidden_path, fpath);
    if (prefix_matches) {
      contains_forbidden_path = true;
    }
    is_forbidden_path |= !strcmp(forbidden_path, fpath);
    if (is_forbidden_path) {
      break;
    }
  }

  /* if this is a forbidden path, do not allow it or children, just move on */
  if (is_forbidden_path) {
    return FTW_SKIP_SUBTREE;
  }

  /* do not allow or continue beneath symbolic links */
  if (typeflag == FTW_SL) {
    return FTW_SKIP_SUBTREE;
  }

  /* if contains forbidden path, allow shallowly and process children */
  if (contains_forbidden_path) {
    int ret = allow_path(fpath, ctx.ruleset_fd, true);
    if (ret < 0) {
      ctx.error = ret;
      return FTW_STOP;
    }

    return FTW_CONTINUE;
  }

  /* allow whole dir or file */
  int ret = allow_path(fpath, ctx.ruleset_fd, false);
  if (ret < 0) {
    ctx.error = ret;
    return FTW_STOP;
  }

  /* do not inspect individual children if this is an allowed dir */
  if (typeflag == FTW_D || typeflag == FTW_DNR) {
    return FTW_SKIP_SUBTREE;
  } else {
    return FTW_CONTINUE;
  }
}

/* one-argument wrapper for realpath */
static char *realpath_alloc(const char *path) { return realpath(path, NULL); }

int forbid_paths(const char **paths, const int ruleset_fd) {
  /* normalize away symbolic links in the forbidden paths. if multiple routes to
   * a file were allowed to exist (as permitted by links), we would allow the
   * one not mentioned by name as a forbidden path, contrary to our intent. */
  char **real_paths = strvmap(paths, realpath_alloc);
  ctx.forbidden_paths = (const char **)real_paths;
  ctx.ruleset_fd = ruleset_fd;
  ctx.error = 0;
  nftw("/", forbid_deep_or_shallow, 512, FTW_PHYS | FTW_ACTIONRETVAL);
  strvfree(real_paths);
  return ctx.error;
}

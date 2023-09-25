#include "landlock.h"
#include "forbid_paths.h"
#include "landlock_syscall.h"

#include <errno.h>
#include <stdio.h>

int landlock_setup(struct landlock_ctx *ctx) {
  int ruleset_fd, abi_ver;
  ctx->ruleset_attr.handled_access_fs = ACCESS_FS_ROUGHLY_READ | ACCESS_FS_ROUGHLY_WRITE;

  abi_ver = landlock_create_ruleset(NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
  if (abi_ver < 0) {
    const int err = errno;

    perror("Failed to check Landlock compatibility");
    switch (err) {
    case ENOSYS:
      fprintf(stderr,
              "Hint: Landlock is not supported by the current kernel. "
              "To support it, build the kernel with "
              "CONFIG_SECURITY_LANDLOCK=y and prepend "
              "\"landlock,\" to the content of CONFIG_LSM.\n");
      break;
    case EOPNOTSUPP:
      fprintf(stderr,
              "Hint: Landlock is currently disabled. "
              "It can be enabled in the kernel configuration by "
              "prepending \"landlock,\" to the content of CONFIG_LSM, "
              "or at boot time by setting the same content to the "
              "\"lsm\" kernel parameter.\n");
      break;
    }
    return 1;
  }

  switch (abi_ver) {
  case 1:
    /*
     * Removes LANDLOCK_ACCESS_FS_REFER for ABI < 2
     *
     * Note: The "refer" operations (file renaming and linking
     * across different directories) are always forbidden when using
     * Landlock with ABI 1.
     *
     * If only ABI 1 is available, this sandboxer knowingly forbids
     * refer operations.
     *
     * If a program *needs* to do refer operations after enabling
     * Landlock, it can not use Landlock at ABI level 1.  To be
     * compatible with different kernel versions, such programs
     * should then fall back to not restrict themselves at all if
     * the running kernel only supports ABI 1.
     */
    ctx->ruleset_attr.handled_access_fs &= ~LANDLOCK_ACCESS_FS_REFER_OR_0;
    __attribute__((fallthrough));
  case 2:
    /* Removes LANDLOCK_ACCESS_FS_TRUNCATE for ABI < 3 */
    ctx->ruleset_attr.handled_access_fs &= ~LANDLOCK_ACCESS_FS_TRUNCATE_OR_0;

    fprintf(stderr,
            "Hint: You should update the running kernel "
            "to leverage Landlock features "
            "provided by ABI version %d (instead of %d).\n",
            LANDLOCK_ABI_LAST, abi_ver);
    __attribute__((fallthrough));
  case LANDLOCK_ABI_LAST:
    break;
  default:
    fprintf(
        stderr,
        "rebuild sandboxer to use features from ABI version %d instead of %d\n",
        abi_ver, LANDLOCK_ABI_LAST);
  }
  return 0;
}

int landlock_apply_path_blacklist(const struct landlock_ctx *ctx, const char **paths) {
  int ruleset_fd = landlock_create_ruleset(&ctx->ruleset_attr, sizeof(ctx->ruleset_attr), 0);
  if (ruleset_fd < 0) {
    perror("Failed to create ruleset");
    return 1;
  }

  if (forbid_paths(paths, ruleset_fd) < 0) {
    fprintf(stderr, "Failed to set up path allowlist\n");
    return 1;
  }

  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    perror("Failed to restrict privileges");
    goto err_close_ruleset;
  }
  if (landlock_restrict_self(ruleset_fd, 0)) {
    perror("Failed to enforce ruleset");
    goto err_close_ruleset;
  }
  close(ruleset_fd);
  return 0;

err_close_ruleset:
  close(ruleset_fd);
  return 1;
}

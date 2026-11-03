/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_USER_SHELL_H
#define LAMIX_USER_SHELL_H

#include <stddef.h>

#define LAMIX_SHELL_MAX_ARGS 16u
#define LAMIX_SHELL_MAX_LINE 256u

typedef enum lamix_shell_status {
  LAMIX_SHELL_OK,
  LAMIX_SHELL_INVALID,
  LAMIX_SHELL_TOO_MANY_ARGS,
  LAMIX_SHELL_UNTERMINATED_QUOTE,
  LAMIX_SHELL_BAD_ESCAPE
} lamix_shell_status;

/* Splits a command line in place. Single/double quotes and backslash escapes
 * are supported; shell expansion, pipes, redirects and command chaining are
 * intentionally not parsed. */
lamix_shell_status lamix_shell_parse(char *line, size_t length,
                                     char *argv[LAMIX_SHELL_MAX_ARGS + 1],
                                     size_t *argc);

#endif

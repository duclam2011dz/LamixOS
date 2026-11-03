/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/shell.h"

lamix_shell_status lamix_shell_parse(char *line, size_t length,
                                     char *argv[LAMIX_SHELL_MAX_ARGS + 1],
                                     size_t *argc) {
  if (!line || !argv || !argc || length > LAMIX_SHELL_MAX_LINE)
    return LAMIX_SHELL_INVALID;
  *argc = 0;
  size_t read = 0, write = 0;
  while (read < length && line[read]) {
    while (read < length && (line[read] == ' ' || line[read] == '\t'))
      ++read;
    if (read == length || !line[read])
      break;
    if (*argc == LAMIX_SHELL_MAX_ARGS)
      return LAMIX_SHELL_TOO_MANY_ARGS;
    argv[(*argc)++] = line + write;
    char quote = 0;
    while (read < length && line[read]) {
      char c = line[read++];
      if (c == '\\' && quote != '\'') {
        if (read == length || !line[read])
          return LAMIX_SHELL_BAD_ESCAPE;
        line[write++] = line[read++];
        continue;
      }
      if (quote) {
        if (c == quote)
          quote = 0;
        else
          line[write++] = c;
        continue;
      }
      if (c == '\'' || c == '"') {
        quote = c;
        continue;
      }
      if (c == ' ' || c == '\t')
        break;
      line[write++] = c;
    }
    if (quote)
      return LAMIX_SHELL_UNTERMINATED_QUOTE;
    line[write++] = 0;
  }
  argv[*argc] = 0;
  return LAMIX_SHELL_OK;
}

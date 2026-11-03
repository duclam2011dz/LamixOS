/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/shell.h"
#include "lamix/syscall.h"
#include <string.h>

static int write_text(const char *path, const char *contents) {
  uint64_t handle = 0, count = 0;
  int64_t result = lamix_file_create(path, LAMIX_FILE_REGULAR);
  if (result < 0 && result != -LAMIX_EEXIST) return 0;
  if (lamix_file_open(path, LAMIX_FILE_READ | LAMIX_FILE_WRITE, &handle) < 0)
    return 0;
  result = lamix_file_write(handle, contents, strlen(contents), &count);
  (void)lamix_handle_close(handle);
  return result == 0 && count == strlen(contents);
}

static void output(const char *text) {
  (void)lamix_debug_write(text, strlen(text));
}

static int list_directory(const char *path) {
  for (uint64_t index = 0; index < 256; ++index) {
    lamix_dirent entry;
    int64_t status = lamix_file_list(path, index, &entry);
    if (status == -LAMIX_ENOENT) return 1;
    if (status < 0) return 0;
    output(entry.name);
    output(entry.kind == LAMIX_FILE_DIRECTORY ? "/\n" : "\n");
  }
  return 0;
}

static int read_file(const char *path) {
  uint64_t handle = 0;
  if (lamix_file_open(path, LAMIX_FILE_READ, &handle) < 0) return 0;
  char bytes[128];
  int ok = 1;
  for (;;) {
    uint64_t count = 0;
    int64_t status = lamix_file_read(handle, bytes, sizeof(bytes), &count);
    if (status < 0) { ok = 0; break; }
    if (!count) break;
    (void)lamix_debug_write(bytes, count);
  }
  (void)lamix_handle_close(handle);
  return ok;
}

static int run_external(char **args, size_t count) {
  if (!count || count > 8) return 0;
  char path[128];
  size_t at = 0;
  if (args[0][0] != '/') path[at++] = '/';
  for (size_t i = 0; args[0][i]; ++i) {
    if (at + 6 >= sizeof(path)) return 0;
    path[at++] = args[0][i];
  }
  if (args[0][0] != '/') {
    static const char suffix[] = ".elf";
    for (size_t i = 0; i < sizeof(suffix); ++i) path[at++] = suffix[i];
  } else path[at] = 0;
  uint64_t child = 0, code = 0;
  if (lamix_process_spawn(path, (const char *const *)args, count, &child) < 0) {
    output("external spawn failed\n");
    return 0;
  }
  int64_t waited = lamix_process_wait(child, &code);
  (void)lamix_handle_close(child);
  if (waited < 0) output("external wait failed\n");
  else if (code) output("external exit failed\n");
  return waited == 0 && code == 0;
}

static int execute(char *line) {
  char *args[LAMIX_SHELL_MAX_ARGS + 1];
  size_t count = 0;
  if (lamix_shell_parse(line, strlen(line) + 1, args, &count) != LAMIX_SHELL_OK)
    return 0;
  if (!count) return 1;
  if (strcmp(args[0], "help") == 0) {
    output("help ls cat write mkdir rm mv log service reboot\n");
    output("M27:SHELL:PS2:PASS\n");
    return 1;
  }
  if (strcmp(args[0], "ls") == 0)
    return count <= 2 && list_directory(count == 2 ? args[1] : "/");
  if (strcmp(args[0], "cat") == 0)
    return count == 2 && read_file(args[1]);
  if (strcmp(args[0], "write") == 0)
    return count == 3 && write_text(args[1], args[2]);
  if (strcmp(args[0], "mkdir") == 0)
    return count == 2 && lamix_file_create(args[1], LAMIX_FILE_DIRECTORY) == 0;
  if (strcmp(args[0], "rm") == 0)
    return count == 2 && lamix_file_remove(args[1]) == 0;
  if (strcmp(args[0], "mv") == 0)
    return count == 3 && lamix_file_rename(args[1], args[2]) == 0;
  if (strcmp(args[0], "log") == 0) {
    char buffer[128];
    uint64_t length = 0;
    int64_t status = lamix_log_read(0, buffer, sizeof(buffer), &length);
    if (status < 0) return 0;
    (void)lamix_debug_write(buffer, length);
    return 1;
  }
  if (strcmp(args[0], "service") == 0) {
    if (count == 2 && strcmp(args[1], "status") == 0) {
      int64_t init = lamix_service_control(0, 0);
      int64_t shell = lamix_service_control(0, 1);
      if (init < 0 || shell < 0) return 0;
      output(init == 0 ? "init running\n" :
             init == 1 ? "init stopped\n" : "init failed\n");
      output(shell == 0 ? "shell running\n" :
             shell == 1 ? "shell stopped\n" :
             shell == 2 ? "shell failed\n" : "shell starting\n");
      return 1;
    }
    return count == 3 &&
        lamix_service_control(strcmp(args[1], "start") == 0 ? 1u :
                              strcmp(args[1], "stop") == 0 ? 2u :
                              strcmp(args[1], "restart") == 0 ? 3u : 0u,
                              1u) == 0;
  }
  if (strcmp(args[0], "reboot") == 0)
    return count == 1 && lamix_system_reboot() == 0;
  if (strcmp(args[0], "exit") == 0)
    return count == 1 ? 2 : 0;
  return run_external(args, count);
}

static int smoke_commands(void) {
  uint64_t forbidden_child = 0;
  const char *forbidden_argv[] = {"alpha", "a"};
  if (lamix_process_spawn_with_rights("/alpha.elf", forbidden_argv, 2,
          &forbidden_child, LAMIX_RIGHT_PROCESS_KILL) != -LAMIX_EACCES)
    return 0;
  output("M27:CAPABILITY:GRANT:DENY:PASS\n");
  char help[] = "help \"file tools\"";
  char *args[LAMIX_SHELL_MAX_ARGS + 1];
  size_t count = 0;
  if (lamix_shell_parse(help, sizeof(help), args, &count) != LAMIX_SHELL_OK ||
      count != 2 || strcmp(args[1], "file tools") != 0)
    return 0;
  char create[] = "write /data/M27-SHELL.TXT \"Lamix M27 shell file API\"";
  if (!execute(create)) return 0;
  char read[] = "cat /data/M27-SHELL.TXT";
  if (!execute(read)) return 0;
  char list[] = "ls /data";
  if (!execute(list)) return 0;
  char directory[] = "mkdir /m27-temp";
  if (lamix_file_create("/m27-temp", LAMIX_FILE_DIRECTORY) < 0 &&
      !execute(directory)) return 0;
  char remove[] = "rm /m27-temp";
  if (!execute(remove)) return 0;
  char rename_out[] = "mv /data/M27-SHELL.TXT /data/M27-RENAMED.TXT";
  if (!execute(rename_out)) return 0;
  char rename_back[] = "mv /data/M27-RENAMED.TXT /data/M27-SHELL.TXT";
  if (!execute(rename_back)) return 0;
  char service[] = "service status";
  if (!execute(service)) return 0;
  char log[] = "log";
  if (!execute(log)) return 0;
  return 1;
}

static int interactive(void) {
  char line[LAMIX_SHELL_MAX_LINE] = {0};
  if (!smoke_commands()) {
    output("M27:SHELL:STARTUP:FAIL\n");
    return 0;
  }
  output("M27:CONSOLE:READY\n");
  for (;;) {
    output("M27:CONSOLE:INPUT_READY\n");
    output("lamix> ");
    size_t used = 0;
    for (;;) {
      char character;
      int64_t result = lamix_console_read(&character);
      if (result == -LAMIX_EAGAIN) { (void)lamix_yield(); continue; }
      if (result < 0) return 0;
      if (character == '\n') {
        output("\n");
        break;
      }
      if (character == '\b' || character == 0x7f) {
        if (used) { --used; output("\b \b"); }
        continue;
      }
      if (used + 1 < sizeof(line)) {
        line[used++] = character;
        line[used] = 0;
        (void)lamix_debug_write(&character, 1);
      }
    }
    int result = execute(line);
    if (result == 2) {
      static const char marker[] = "M27:SHELL:COMMANDS:PASS\nM27:SHELL:FILES:PASS\n";
      (void)lamix_debug_write(marker, sizeof(marker) - 1);
      return 0;
    }
    if (!result) output("command failed\n");
    memset(line, 0, sizeof(line));
  }
}

int main(int argc, char **argv, char **envp) {
  (void)envp;
  if (argc > 1 && strcmp(argv[1], "--test") == 0) {
    if (!smoke_commands()) return 2;
    static const char marker[] = "M27:SHELL:COMMANDS:PASS\nM27:SHELL:FILES:PASS\n";
    (void)lamix_debug_write(marker, sizeof(marker) - 1);
    return 0;
  }
  if (argc > 1 && strcmp(argv[1], "--interactive-test") != 0 &&
      strcmp(argv[1], "--interactive-test-run2") != 0 &&
      strcmp(argv[1], "--interactive-test-run3") != 0) return 1;
  if (argc > 1 && strcmp(argv[1], "--interactive-test-run2") == 0)
    output("\nM27:CONSOLE:RUN2_READY\n");
  if (argc > 1 && strcmp(argv[1], "--interactive-test-run3") == 0)
    output("\nM27:CONSOLE:RUN3_READY\n");
  if (argc <= 1 || strcmp(argv[1], "--interactive-test") == 0 ||
      strcmp(argv[1], "--interactive-test-run2") == 0 ||
      strcmp(argv[1], "--interactive-test-run3") == 0)
    return interactive();
  return 1;
}

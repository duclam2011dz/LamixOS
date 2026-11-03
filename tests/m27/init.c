/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/services.h"
#include "lamix/syscall.h"

static const lamix_service_spec services[] = {
    {"shell", "/shell.elf", 0, LAMIX_SERVICE_RESTART_LIMIT,
     LAMIX_RIGHT_FILE_READ | LAMIX_RIGHT_FILE_WRITE |
     LAMIX_RIGHT_FILE_ENUM | LAMIX_RIGHT_SERVICE_CONTROL |
     LAMIX_RIGHT_SYSTEM_REBOOT | LAMIX_RIGHT_LOG_READ |
     LAMIX_RIGHT_PROCESS_SPAWN}};

static int command_is(const char *line, const char *command) {
  size_t i = 0;
  while (line[i] && command[i] && line[i] == command[i]) ++i;
  return line[i] == 0 && command[i] == 0;
}

static void wait_for_start_request(void) {
  static const char prompt[] = "service stopped; type service start shell\n> ";
  (void)lamix_debug_write(prompt, sizeof(prompt) - 1);
  static const char stopped_marker[] = "\nM27:INIT:SERVICE:INPUT_READY\n";
  (void)lamix_debug_write(stopped_marker, sizeof(stopped_marker) - 1);
  char line[64];
  for (;;) {
    size_t used = 0;
    for (;;) {
      char character = 0;
      int64_t status = lamix_console_read(&character);
      if (status == -LAMIX_EAGAIN) { (void)lamix_yield(); continue; }
      if (status < 0) return;
      if (character == '\r') continue;
      if (character == '\n') break;
      if (used + 1 < sizeof(line)) line[used++] = character;
    }
    line[used] = 0;
    if (command_is(line, "service start shell")) {
      (void)lamix_service_control(1, 1);
      return;
    }
    if (command_is(line, "service restart shell")) {
      (void)lamix_service_control(3, 1);
      return;
    }
    static const char denied[] = "unsupported init command\n> ";
    (void)lamix_debug_write(denied, sizeof(denied) - 1);
  }
}

int main(int argc, char **argv, char **envp) {
  (void)argc; (void)argv; (void)envp;
  uint8_t order[sizeof(services) / sizeof(services[0])];
  if (lamix_services_order(services, sizeof(services) / sizeof(services[0]),
                           order) != LAMIX_SERVICES_OK)
    return 1;
  static const char marker[] = "M27:INIT:GRAPH:PASS\n";
  (void)lamix_debug_write(marker, sizeof(marker) - 1);
  lamix_service_runtime runtime = {.state = LAMIX_SERVICE_READY};
  uint64_t tick = 0;
  uint32_t launches = 0;
  for (;;) {
    int64_t desired = lamix_service_control(0, 1);
    if (desired == 1) {
      runtime.state = LAMIX_SERVICE_STOPPED;
      wait_for_start_request();
      continue;
    }
    if (desired < 0) return 3;
    uint64_t child = 0, exit_code = 0;
    const char *shell_mode = launches == 0 ? "--interactive-test" :
                             launches == 1 ? "--interactive-test-run2" :
                                             "--interactive-test-run3";
    const char *shell_argv[] = {"shell", shell_mode};
    int64_t status = lamix_process_spawn_with_rights(
        services[order[0]].path, shell_argv, 2, &child,
        services[order[0]].permissions);
    if (status < 0) {
      runtime.state = LAMIX_SERVICE_FAILED;
      uint32_t delay = 0;
      if (!lamix_service_restart_due(&runtime,
              services[order[0]].restart_limit, tick, &delay)) return 2;
      tick += delay;
      for (uint32_t i = 0; i < delay; ++i) (void)lamix_yield();
      continue;
    }
    if (++launches > 1) {
      static const char restarted[] = "M27:INIT:SERVICE:RESTARTED\n";
      (void)lamix_debug_write(restarted, sizeof(restarted) - 1);
    }
    runtime.state = LAMIX_SERVICE_RUNNING;
    status = lamix_process_wait(child, &exit_code);
    (void)lamix_handle_close(child);
    if (status == 0 && exit_code == 0) {
      runtime.state = LAMIX_SERVICE_STOPPED;
      static const char marker_reaped[] = "M27:INIT:SERVICE:REAP:PASS\n";
      (void)lamix_debug_write(marker_reaped, sizeof(marker_reaped) - 1);
      return 0;
    }
    desired = lamix_service_control(0, 1);
    if (desired == 1) continue;
    if (desired == 2 || desired == 3) continue;
    runtime.state = LAMIX_SERVICE_FAILED;
    uint32_t delay = 0;
    if (!lamix_service_restart_due(&runtime,
            services[order[0]].restart_limit, tick, &delay)) return 2;
    tick += delay;
    for (uint32_t i = 0; i < delay; ++i) (void)lamix_yield();
  }
}

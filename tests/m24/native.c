/* SPDX-License-Identifier: GPL-3.0-only */
#include "process.h"
#include "acpi.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct host_context {
  uint32_t cpu;
  uint32_t switches;
} host_context;

static void *allocate(void *context, size_t size) {
  (void)context;
  return malloc(size);
}
static void deallocate(void *context, void *pointer) {
  (void)context;
  free(pointer);
}
static uintptr_t irq_save(void *context) {
  (void)context;
  return 0;
}
static void irq_restore(void *context, uintptr_t flags) {
  (void)context;
  (void)flags;
}
static int reschedule(void *context) {
  (void)context;
  return 1;
}
static void fatal(void *context, uint64_t reason) {
  (void)context;
  (void)reason;
  abort();
}
static uint32_t host_cpu_id(void *context) {
  return ((host_context *)context)->cpu;
}
static void context_switch(void *context, scheduler_thread *next) {
  (void)next;
  ++((host_context *)context)->switches;
}
static void idle(void *argument) { (void)argument; }
static void worker(void *argument) { (void)argument; }

static void put32(uint8_t *p, uint32_t value) {
  p[0] = (uint8_t)value;
  p[1] = (uint8_t)(value >> 8);
  p[2] = (uint8_t)(value >> 16);
  p[3] = (uint8_t)(value >> 24);
}
static void fix_checksum(uint8_t *p, size_t size) {
  p[9] = 0;
  uint8_t sum = 0;
  for (size_t i = 0; i < size; ++i) sum = (uint8_t)(sum + p[i]);
  p[9] = (uint8_t)(0u - sum);
}
static void test_madt_cpu_inventory(void) {
  uint8_t madt[44 + 8 + 8 + 8 + 12 + 16] = {0};
  memcpy(madt, "APIC", 4);
  put32(madt + 4, sizeof(madt));
  put32(madt + 36, UINT32_C(0xfee00000));
  size_t at = 44;
  madt[at] = 0; madt[at + 1] = 8; madt[at + 2] = 0;
  madt[at + 3] = 2; put32(madt + at + 4, 1); at += 8;
  madt[at] = 0; madt[at + 1] = 8; madt[at + 2] = 1;
  madt[at + 3] = 7; put32(madt + at + 4, 1); at += 8;
  madt[at] = 0; madt[at + 1] = 8; madt[at + 2] = 2;
  madt[at + 3] = 9; put32(madt + at + 4, 0); at += 8;
  madt[at] = 1; madt[at + 1] = 12;
  put32(madt + at + 4, UINT32_C(0xfec00000)); at += 12;
  madt[at] = 9; madt[at + 1] = 16;
  put32(madt + at + 8, 0); at += 16;
  assert(at == sizeof(madt));
  fix_checksum(madt, sizeof(madt));
  acpi_madt_info info;
  assert(acpi_madt_parse(madt, sizeof(madt), 2, &info) == ACPI_OK);
  assert(info.cpu_count == 2 && info.bsp_present && !info.x2apic_present);
  assert(info.cpus[0].processor_id == 0 && info.cpus[0].apic_id == 2 &&
         info.cpus[1].processor_id == 1 && info.cpus[1].apic_id == 7);
  assert(acpi_madt_parse(madt, sizeof(madt) - 1, 2, &info) ==
         ACPI_BAD_CHECKSUM);
  put32(madt + sizeof(madt) - 8, 1);
  fix_checksum(madt, sizeof(madt));
  assert(acpi_madt_parse(madt, sizeof(madt), 2, &info) == ACPI_OK);
  assert(info.x2apic_present);
  madt[44 + 8 + 3] = 2;
  fix_checksum(madt, sizeof(madt));
  assert(acpi_madt_parse(madt, sizeof(madt), 2, &info) == ACPI_BAD_ENTRY);
}

static void test_process_teardown_waits_for_active_cpu(void) {
  process instance = {0};
  instance.state = PROCESS_EXITED;
  instance.address_space.active_cpus = UINT64_C(1) << 2;
  assert(process_destroy(&instance) == PROCESS_BUSY);
}

int main(void) {
  test_madt_cpu_inventory();
  test_process_teardown_waits_for_active_cpu();
  host_context host = {0};
  scheduler state;
  scheduler_operations operations = {
      &host, allocate, deallocate, irq_save, irq_restore, reschedule,
      fatal, context_switch, host_cpu_id};
  assert(scheduler_init_smp(&state, &operations, idle, 4) == SCHED_OK);
  assert(scheduler_begin(&state) == SCHED_OK);
  for (uint32_t cpu = 1; cpu < 4; ++cpu) {
    host.cpu = cpu;
    assert(scheduler_cpu_online(&state, cpu,
                                UINT64_C(0xffff800000000000) +
                                    (uint64_t)cpu * 0x10000u) == SCHED_OK);
  }
  host.cpu = 0;

  scheduler_thread *stealable = 0;
  assert(scheduler_create(&state, worker, 0, 1, &stealable) == SCHED_OK);
  assert(stealable && stealable->run_cpu == 0);
  stealable->user_mode = 1;
  assert(scheduler_set_affinity(stealable, UINT64_C(3)) == SCHED_OK);

  scheduler_thread *pinned = 0;
  assert(scheduler_create(&state, worker, 0, 1, &pinned) == SCHED_OK);
  assert(pinned);
  assert(scheduler_set_affinity(pinned, UINT64_C(1)) == SCHED_OK);
  assert(pinned->run_cpu == 0);

  host.cpu = 1;
  scheduler_frame idle_frame = {0};
  (void)scheduler_on_tick(&state, &idle_frame);
  assert(scheduler_current(&state) == stealable);
  assert(stealable->run_cpu == 1);
  assert(scheduler_work_steals(&state) == 1);
  assert(scheduler_observed_cpu_mask(&state) == UINT64_C(2));
  assert(pinned->run_cpu == 0);
  assert(host.switches == 1);

  host.cpu = 3;
  assert(scheduler_set_affinity(stealable, UINT64_C(8)) == SCHED_OK);
  assert(scheduler_current(&state) == state.cpus[2].current);
  assert(scheduler_set_affinity(stealable, UINT64_C(1) << 4) == SCHED_INVALID);
  assert(scheduler_tick_count(&state) == 0);
  return 0;
}

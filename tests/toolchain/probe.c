/* SPDX-License-Identifier: GPL-3.0-only */
/* Sample freestanding; không headers/runtime host. */
extern unsigned long long m01_cpp_probe(unsigned long long value);
unsigned long long m01_c_probe(void) {
    return m01_cpp_probe(41);
}

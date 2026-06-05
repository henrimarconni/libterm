/*
 * src/intrinsics/dispatch.c
 *
 * Runtime SIMD backend selection. Compiled only when LIBTERM_SIMD=auto
 * resolves to more than one candidate backend; each backend TU is then built
 * with its lt__simd_* symbols renamed to lt__simd_*_<backend> (CMake compile
 * definitions), and this TU owns the canonical names as forwarders through
 * function pointers. The pointers start at the architecture baseline
 * (always compiled), so they are valid even before the constructor runs;
 * the constructor only ever upgrades. Alongside common.h, this is the one
 * intrinsics file allowed per-OS/arch conditionals.
 */
#include "dispatch.h"
#include "diff.h" /* canonical prototypes — keeps the forwarders honest */

#include <string.h>

#if defined(__linux__) && (defined(__aarch64__) || defined(__riscv))
#include <sys/auxv.h>
#ifndef HWCAP_SVE
#define HWCAP_SVE (1UL << 22)
#endif
#endif

/* Baseline per architecture: NEON is part of the ARMv8 baseline; everything
 * else falls back to plain C. CMake guarantees the baseline backend is in
 * every dispatch build's backend list. */
#if defined(__aarch64__)
#define LT__SIMD_BASELINE neon
#define LT__SIMD_BASELINE_NAME "neon"
#else
#define LT__SIMD_BASELINE scalar
#define LT__SIMD_BASELINE_NAME "scalar"
#endif

#define LT__SIMD_CAT2(a, b) a##_##b
#define LT__SIMD_CAT(a, b) LT__SIMD_CAT2(a, b)

static lt__simd_diff_fn lt__differ =
    LT__SIMD_CAT(lt__simd_diff_first_differ_cell, LT__SIMD_BASELINE);
static lt__simd_diff_fn lt__equal =
    LT__SIMD_CAT(lt__simd_diff_first_equal_cell, LT__SIMD_BASELINE);
static lt__simd_fill_fn lt__fill =
    LT__SIMD_CAT(lt__simd_fill_cells, LT__SIMD_BASELINE);
static const char *lt__active = LT__SIMD_BASELINE_NAME;

int lt__simd_backend_supported(const char *name) {
  if (strcmp(name, LT__SIMD_BASELINE_NAME) == 0)
    return 1;
#if defined(LT_SIMD_HAVE_AVX2) && defined(__x86_64__)
  if (strcmp(name, "avx2") == 0)
    return __builtin_cpu_supports("avx2") != 0;
#endif
#if defined(LT_SIMD_HAVE_AVX512) && defined(__x86_64__)
  if (strcmp(name, "avx512") == 0)
    return __builtin_cpu_supports("avx512f") &&
           __builtin_cpu_supports("avx512bw");
#endif
#if defined(LT_SIMD_HAVE_SVE) && defined(__linux__)
  if (strcmp(name, "sve") == 0)
    return (getauxval(AT_HWCAP) & HWCAP_SVE) != 0;
#endif
#if defined(LT_SIMD_HAVE_RVV) && defined(__linux__)
  /* The kernel's COMPAT_HWCAP_ISA_V bit ('V' extension). */
  if (strcmp(name, "rvv") == 0)
    return (getauxval(AT_HWCAP) & (1UL << ('V' - 'A'))) != 0;
#endif
  return 0;
}

__attribute__((constructor)) static void lt__simd_select(void) {
#if defined(__x86_64__)
  __builtin_cpu_init();
#endif
#ifdef LT_SIMD_HAVE_AVX512
  if (lt__simd_backend_supported("avx512")) {
    lt__differ = lt__simd_diff_first_differ_cell_avx512;
    lt__equal = lt__simd_diff_first_equal_cell_avx512;
    lt__fill = lt__simd_fill_cells_avx512;
    lt__active = "avx512";
    return;
  }
#endif
#ifdef LT_SIMD_HAVE_AVX2
  if (lt__simd_backend_supported("avx2")) {
    lt__differ = lt__simd_diff_first_differ_cell_avx2;
    lt__equal = lt__simd_diff_first_equal_cell_avx2;
    lt__fill = lt__simd_fill_cells_avx2;
    lt__active = "avx2";
    return;
  }
#endif
#ifdef LT_SIMD_HAVE_SVE
  if (lt__simd_backend_supported("sve")) {
    lt__differ = lt__simd_diff_first_differ_cell_sve;
    lt__equal = lt__simd_diff_first_equal_cell_sve;
    lt__fill = lt__simd_fill_cells_sve;
    lt__active = "sve";
    return;
  }
#endif
#ifdef LT_SIMD_HAVE_RVV
  if (lt__simd_backend_supported("rvv")) {
    lt__differ = lt__simd_diff_first_differ_cell_rvv;
    lt__equal = lt__simd_diff_first_equal_cell_rvv;
    lt__fill = lt__simd_fill_cells_rvv;
    lt__active = "rvv";
    return;
  }
#endif
}

const char *lt__simd_active_backend(void) { return lt__active; }

/* Canonical entry points (the names diff.h declares and shared code calls):
 * forward through the selected backend. */
int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count) {
  return lt__differ(a, b, count);
}

int lt__simd_diff_first_equal_cell(const struct lt_cell *a,
                                   const struct lt_cell *b, int count) {
  return lt__equal(a, b, count);
}

void lt__simd_fill_cells(struct lt_cell *buf, int count, lt_attr fg,
                         lt_attr bg) {
  lt__fill(buf, count, fg, bg);
}

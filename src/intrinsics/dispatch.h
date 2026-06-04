/*
 * src/intrinsics/dispatch.h
 *
 * Declarations for runtime-dispatch builds (LIBTERM_SIMD=auto resolving to
 * more than one candidate backend): per-backend renamed symbols, function
 * pointer types, and internal observability. Included by dispatch.c and the
 * SIMD tests only — backend TUs never include this (their renaming happens
 * via CMake compile definitions).
 */
#ifndef LIBTERM_INTRINSICS_DISPATCH_H
#define LIBTERM_INTRINSICS_DISPATCH_H

#include "libterm/libterm.h"

typedef int (*lt__simd_diff_fn)(const struct lt_cell *a,
                                const struct lt_cell *b, int count);
typedef void (*lt__simd_fill_fn)(struct lt_cell *buf, int count, lt_attr fg,
                                 lt_attr bg);

#define LT__SIMD_DECL_BACKEND(suffix)                                          \
  int lt__simd_diff_first_differ_cell_##suffix(                                \
      const struct lt_cell *a, const struct lt_cell *b, int count);            \
  int lt__simd_diff_first_equal_cell_##suffix(                                 \
      const struct lt_cell *a, const struct lt_cell *b, int count);            \
  void lt__simd_fill_cells_##suffix(struct lt_cell *buf, int count,            \
                                    lt_attr fg, lt_attr bg)

#ifdef LT_SIMD_HAVE_SCALAR
LT__SIMD_DECL_BACKEND(scalar);
#endif
#ifdef LT_SIMD_HAVE_AVX2
LT__SIMD_DECL_BACKEND(avx2);
#endif
#ifdef LT_SIMD_HAVE_AVX512
LT__SIMD_DECL_BACKEND(avx512);
#endif
#ifdef LT_SIMD_HAVE_NEON
LT__SIMD_DECL_BACKEND(neon);
#endif
#ifdef LT_SIMD_HAVE_SVE
LT__SIMD_DECL_BACKEND(sve);
#endif
#ifdef LT_SIMD_HAVE_RVV
LT__SIMD_DECL_BACKEND(rvv);
#endif

/* Name of the backend the startup constructor selected ("scalar", "avx2",
 * "avx512", "neon", "sve", "rvv"). */
const char *lt__simd_active_backend(void);
/* 1 if the named compiled backend can execute on the running CPU. */
int lt__simd_backend_supported(const char *name);

#endif /* LIBTERM_INTRINSICS_DISPATCH_H */

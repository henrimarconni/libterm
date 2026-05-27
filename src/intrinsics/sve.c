/*
 * src/intrinsics/sve.c
 *
 * AArch64 SVE / SVE2 implementation of the SIMD diff contract.
 * Compiled when LIBTERM_SIMD=sve (auto does NOT pick this — explicit opt-in).
 * Compile flag: -march=armv8-a+sve (GCC/Clang).
 *
 * SVE has scalable vector length (128–2048 bits). Predicate registers are
 * first-class: comparisons return predicates, find-first-active is one
 * native instruction (svbrkb_z + svcntp).
 */
#include "intrinsics/common.h"
#include "intrinsics/diff.h"
#include "libterm/libterm.h"

#include <arm_sve.h>
#include <stdint.h>

int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)(const void *)a;
  const char *pb = (const char *)(const void *)b;
  size_t n = (size_t)count * sizeof(struct lt_cell);
  size_t i = 0;
  uint64_t vlen = svcntb(); /* bytes per SVE vector at runtime */
  svbool_t pg = svptrue_b8();
  svuint8_t va = svdup_n_u8(0);
  svuint8_t vb = svdup_n_u8(0);
  svbool_t neq = svpfalse_b();

  while (i + vlen <= n) {
    va = svld1_u8(pg, (const uint8_t *)(pa + i));
    vb = svld1_u8(pg, (const uint8_t *)(pb + i));
    neq = svcmpne_u8(pg, va, vb);
    if (svptest_any(pg, neq)) {
      /* break-before-first-true: predicate of all true lanes preceding the
       * first true in `neq`. svcntp counts how many. */
      svbool_t before = svbrkb_z(pg, neq);
      uint64_t lane = svcntp_b8(pg, before);
      size_t byte_idx = i + (size_t)lane;
      return (int)(byte_idx / sizeof(struct lt_cell));
    }
    i += vlen;
  }

  /* scalar tail for any partial trailing region */
  for (; i < n; i++) {
    if (pa[i] != pb[i])
      return (int)(i / sizeof(struct lt_cell));
  }
  return count;
}

/* SVE first-equal: one 16-byte cell per iteration using a 16-byte predicate.
 * The cell is equal iff no active lane differs. VLEN >= 128 bits is the SVE
 * architectural minimum, so a 16-byte predicate always fits in one vector.
 * Relies on the _reserved==0 invariant. */
int lt__simd_diff_first_equal_cell(const struct lt_cell *a,
                                   const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)(const void *)a;
  const char *pb = (const char *)(const void *)b;
  svbool_t pg = svwhilelt_b8((uint64_t)0, (uint64_t)sizeof(struct lt_cell));
  svuint8_t va = svdup_n_u8(0);
  svuint8_t vb = svdup_n_u8(0);
  svbool_t neq = svpfalse_b();

  for (int i = 0; i < count; i++) {
    const size_t off = (size_t)i * sizeof(struct lt_cell);
    va = svld1_u8(pg, (const uint8_t *)(pa + off));
    vb = svld1_u8(pg, (const uint8_t *)(pb + off));
    neq = svcmpne_u8(pg, va, vb);
    if (!svptest_any(pg, neq))
      return i;
  }
  return count;
}

/* SVE fill: replicate the 128-bit template across the whole scalable vector
 * with svld1rq, then store svcntb()-byte chunks with a predicated final tail.
 * total and every chunk offset are multiples of 16, so the replicated pattern
 * stays cell-aligned. */
void lt__simd_fill_cells(struct lt_cell *buf, int count, lt_attr fg,
                         lt_attr bg) {
  if (count <= 0)
    return;

  struct lt_cell tmpl = {.ch = ' ', .fg = fg, .bg = bg, ._reserved = 0};
  svbool_t all = svptrue_b8();
  svuint8_t v = svld1rq_u8(all, (const uint8_t *)&tmpl);

  uint8_t *p = (uint8_t *)buf;
  size_t total = (size_t)count * sizeof(struct lt_cell);
  size_t vlen = svcntb();
  for (size_t off = 0; off < total; off += vlen) {
    svbool_t pg = svwhilelt_b8((uint64_t)off, (uint64_t)total);
    svst1_u8(pg, p + off, v);
  }
}

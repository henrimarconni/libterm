/*
 * src/intrinsics/rvv.c
 *
 * RISC-V Vector (RVV 1.0) implementation of the SIMD diff contract.
 * Compiled when LIBTERM_SIMD=rvv (auto does NOT pick this — explicit opt-in).
 * Compile flag: -march=rv64gcv (GCC/Clang).
 *
 * RVV has scalable vector length set per loop via vsetvl. Comparisons
 * return mask registers; vfirst gives the index of the first set bit
 * directly.
 */
#include "intrinsics/common.h"
#include "intrinsics/diff.h"
#include "libterm/libterm.h"

#include <riscv_vector.h>
#include <stddef.h>
#include <stdint.h>

int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)(const void *)a;
  const char *pb = (const char *)(const void *)b;
  size_t n = (size_t)count * sizeof(struct lt_cell);
  size_t i = 0;

  vuint8m1_t va, vb;
  vbool8_t neq;
  while (i < n) {
    size_t avl = n - i;
    size_t vl = __riscv_vsetvl_e8m1(avl);

    va = __riscv_vle8_v_u8m1((const uint8_t *)(pa + i), vl);
    vb = __riscv_vle8_v_u8m1((const uint8_t *)(pb + i), vl);
    neq = __riscv_vmsne_vv_u8m1_b8(va, vb, vl);

    /* vfirst returns the index of the first true lane, or -1 if none. */
    long first = __riscv_vfirst_m_b8(neq, vl);
    if (first >= 0) {
      size_t byte_idx = i + (size_t)first;
      return (int)(byte_idx / sizeof(struct lt_cell));
    }

    i += vl;
  }

  return count;
}

/* RVV first-equal: one 16-byte cell per iteration. vsetvl(16) yields vl==16 for
 * VLEN >= 128 (the rv64gcv target). The cell is equal iff no byte differs, i.e.
 * vfirst over the not-equal mask is -1. The 16-byte compare includes _reserved
 * (the cluster id), which is what cell-equality requires. */
int lt__simd_diff_first_equal_cell(const struct lt_cell *a,
                                   const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)(const void *)a;
  const char *pb = (const char *)(const void *)b;
  const size_t cell = sizeof(struct lt_cell);
  vuint8m1_t va, vb;
  vbool8_t neq;

  for (int i = 0; i < count; i++) {
    size_t vl = __riscv_vsetvl_e8m1(cell);
    va = __riscv_vle8_v_u8m1((const uint8_t *)(pa + (size_t)i * cell), vl);
    vb = __riscv_vle8_v_u8m1((const uint8_t *)(pb + (size_t)i * cell), vl);
    neq = __riscv_vmsne_vv_u8m1_b8(va, vb, vl);
    if (__riscv_vfirst_m_b8(neq, vl) < 0)
      return i;
  }
  return count;
}

/* RVV fill: the cell is 4x u32 {ch, fg, bg, _reserved}. Write each field across
 * all cells with a stride-16 store (vsse32) — four vectorized passes cover the
 * whole buffer. vsetvl chunks each pass; no tail handling needed. */
void lt__simd_fill_cells(struct lt_cell *buf, int count, lt_attr fg,
                         lt_attr bg) {
  if (count <= 0)
    return;

  const uint32_t fields[4] = {(uint32_t)' ', (uint32_t)fg, (uint32_t)bg, 0u};

  for (int f = 0; f < 4; f++) {
    size_t idx = 0;
    while (idx < (size_t)count) {
      size_t vl = __riscv_vsetvl_e32m1((size_t)count - idx);
      vuint32m1_t vv = __riscv_vmv_v_x_u32m1(fields[f], vl);
      uint32_t *base = (uint32_t *)((char *)buf + idx * sizeof(struct lt_cell) +
                                    (size_t)f * sizeof(uint32_t));
      __riscv_vsse32_v_u32m1(base, (ptrdiff_t)sizeof(struct lt_cell), vv, vl);
      idx += vl;
    }
  }
}

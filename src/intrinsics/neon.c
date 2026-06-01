/*
 * src/intrinsics/neon.c
 *
 * AArch64 NEON implementation of the SIMD diff contract.
 * Compiled when LIBTERM_SIMD=neon (auto picks this on aarch64).
 * No special compile flag needed — NEON is mandatory in the AArch64 baseline.
 *
 * Key wrinkle: NEON has no native movemask. We compress 16 lanes of byte
 * compare results (each 0xFF or 0x00) into a 64-bit integer using the
 * vshrn-by-4 trick: each byte becomes a 4-bit nibble (0xF or 0x0).
 * find-first-differing-byte = ctz(~mask) / 4.
 */
#include "intrinsics/common.h"
#include "intrinsics/diff.h"
#include "libterm/libterm.h"

#include <arm_neon.h>
#include <stdint.h>

/* Compress 16x8 byte vector (each byte 0xFF or 0x00) to 64-bit nibble mask. */
static inline uint64_t lt__neon_movemask(uint8x16_t v) {
  uint8x8_t narrowed = vshrn_n_u16(vreinterpretq_u16_u8(v), 4);
  return vget_lane_u64(vreinterpret_u64_u8(narrowed), 0);
}

int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)(const void *)a;
  const char *pb = (const char *)(const void *)b;
  size_t n = (size_t)count * sizeof(struct lt_cell);
  size_t i = 0;
  uint8x16_t va = vdupq_n_u8(0);
  uint8x16_t vb = vdupq_n_u8(0);
  uint8x16_t eq = vdupq_n_u8(0);
  uint64_t mask = 0;

  for (; i + 16 <= n; i += 16) {
    va = vld1q_u8((const uint8_t *)(pa + i));
    vb = vld1q_u8((const uint8_t *)(pb + i));
    eq = vceqq_u8(va, vb);
    mask = lt__neon_movemask(eq);
    if (mask != 0xFFFFFFFFFFFFFFFFull) {
      /* each byte became a 4-bit nibble; first differing byte at lane = ctz / 4
       */
      uint64_t diff = ~mask;
      size_t lane = (size_t)lt__ctz64(diff) >> 2; /* /4 */
      size_t byte_idx = i + lane;
      return (int)(byte_idx / sizeof(struct lt_cell));
    }
  }

  /* scalar tail */
  for (; i < n; i++) {
    if (pa[i] != pb[i])
      return (int)(i / sizeof(struct lt_cell));
  }
  return count;
}

/* NEON first-equal: a 128-bit vector is exactly one 16-byte cell. The cell is
 * equal iff all 16 byte-compares are true, i.e. the nibble movemask is
 * all-ones. The 16-byte compare includes _reserved (the cluster id), which is
 * exactly what cell-equality requires: two cells render the same iff ch/fg/bg
 * AND cluster match (ids are content-deduped, so equal id <=> equal cluster).
 */
int lt__simd_diff_first_equal_cell(const struct lt_cell *a,
                                   const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)(const void *)a;
  const char *pb = (const char *)(const void *)b;
  uint8x16_t va = vdupq_n_u8(0);
  uint8x16_t vb = vdupq_n_u8(0);
  uint8x16_t eq = vdupq_n_u8(0);
  uint64_t mask = 0;

  for (int i = 0; i < count; i++) {
    const size_t off = (size_t)i * sizeof(struct lt_cell);
    va = vld1q_u8((const uint8_t *)(pa + off));
    vb = vld1q_u8((const uint8_t *)(pb + off));
    eq = vceqq_u8(va, vb);
    mask = lt__neon_movemask(eq);
    if (mask == 0xFFFFFFFFFFFFFFFFull)
      return i;
  }
  return count;
}

/* NEON fill: load the 16-byte cell template once, store it per cell
 * (each cell is one 128-bit vst1q). */
void lt__simd_fill_cells(struct lt_cell *buf, int count, lt_attr fg,
                         lt_attr bg) {
  if (count <= 0)
    return;

  struct lt_cell tmpl = {.ch = ' ', .fg = fg, .bg = bg, ._reserved = 0};
  uint8x16_t v = vld1q_u8((const uint8_t *)&tmpl);

  char *p = (char *)buf;
  for (int i = 0; i < count; i++)
    vst1q_u8((uint8_t *)(p + (size_t)i * sizeof(struct lt_cell)), v);
}

/*
 * src/intrinsics/avx512.c
 *
 * x86_64 AVX-512 implementation of the SIMD diff contract.
 * Compiled when LIBTERM_SIMD=avx512 (auto does NOT pick this — explicit opt-in).
 * Compile flags set per-TU in src/CMakeLists.txt:
 *   - GCC/Clang: -mavx512f -mavx512bw
 *   - MSVC:      /arch:AVX512
 *
 * AVX-512 has native mask registers, so the byte-equal compare returns a
 * 64-bit mask directly (no movemask trick needed).
 */
#include "intrinsics/common.h"
#include "intrinsics/diff.h"
#include "libterm/libterm.h"

#include <immintrin.h>
#include <stdint.h>

int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)(const void *)a;
  const char *pb = (const char *)(const void *)b;
  size_t n = (size_t)count * sizeof(struct lt_cell);
  size_t i = 0;
  __m512i va = _mm512_setzero_si512();
  __m512i vb = _mm512_setzero_si512();
  __mmask64 eq = 0;

  for (; i + 64 <= n; i += 64) {
    va = _mm512_loadu_si512((const void *)(pa + i));
    vb = _mm512_loadu_si512((const void *)(pb + i));
    eq = _mm512_cmpeq_epu8_mask(va, vb);
    if (eq != 0xFFFFFFFFFFFFFFFFull) {
      uint64_t diff = ~(uint64_t)eq;
      size_t byte_idx = i + (size_t)lt__ctz64(diff);
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

/* AVX-512 first-equal: a 512-bit vector holds 4 cells. cmpeq gives a 64-bit
 * mask; cell c (of the 4) is equal iff its 16 mask bits are all set. Relies on
 * the _reserved==0 invariant (16-byte byte-equality == field-equality). */
int lt__simd_diff_first_equal_cell(const struct lt_cell *a,
                                   const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)(const void *)a;
  const char *pb = (const char *)(const void *)b;
  int groups = count / 4;
  __m512i va = _mm512_setzero_si512();
  __m512i vb = _mm512_setzero_si512();
  __mmask64 eq = 0;

  for (int g = 0; g < groups; g++) {
    va = _mm512_loadu_si512((const void *)(pa + (size_t)g * 64));
    vb = _mm512_loadu_si512((const void *)(pb + (size_t)g * 64));
    eq = _mm512_cmpeq_epu8_mask(va, vb);
    uint64_t m = (uint64_t)eq;
    for (int c = 0; c < 4; c++) {
      if (((m >> (16 * c)) & 0xFFFFu) == 0xFFFFu)
        return g * 4 + c;
    }
  }

  /* tail: up to 3 cells, scalar */
  for (int i = groups * 4; i < count; i++) {
    /* Include _reserved (cluster id) to match the byte-wise vector path above. */
    if (a[i].ch == b[i].ch && a[i].fg == b[i].fg && a[i].bg == b[i].bg &&
        a[i]._reserved == b[i]._reserved)
      return i;
  }
  return count;
}

/* AVX-512 fill: broadcast the 16-byte template into all four 128-bit lanes
 * (= 4 cells) and store 64 bytes per iteration; tail handled scalar. */
void lt__simd_fill_cells(struct lt_cell *buf, int count, lt_attr fg,
                         lt_attr bg) {
  if (count <= 0)
    return;

  struct lt_cell tmpl = {.ch = ' ', .fg = fg, .bg = bg, ._reserved = 0};
  __m128i x = _mm_loadu_si128((const __m128i *)&tmpl);
  __m512i v = _mm512_broadcast_i32x4(x);

  char *p = (char *)buf;
  int groups = count / 4;
  for (int g = 0; g < groups; g++)
    _mm512_storeu_si512((void *)(p + (size_t)g * 64), v);

  for (int i = groups * 4; i < count; i++)
    buf[i] = tmpl;
}

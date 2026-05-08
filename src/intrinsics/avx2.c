/*
 * src/intrinsics/avx2.c
 *
 * x86_64 AVX2 implementation of the SIMD diff contract.
 * Compiled when LIBTERM_SIMD=avx2 (or auto resolves to it)
 * Compile flag set per-TU in src/CMakeLists.txt
 *
 *   - GCC/Clang: -mavx2
 *   - MSVC:      /arch:AVX2
 */

#include "common.h"
#include "diff.h"
#include "libterm/libterm.h"

#include <immintrin.h>
#include <stdint.h>

/* Cell layout: {u32 ch, u32 fg, u32 bg, u32 _reserved} = 16 bytes.
 * AVX2 ymm = 32 bytes = 2 cells exactly. Byte-equal across [0, b)
 * implies cell-equal for cells fully in [0, b); byte-level SIMD scan
 * + (byte_idx / 16) gives the correct first-differing-cell index. */

int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)a;
  const char *pb = (const char *)b;
  size_t n = (size_t)count * sizeof(struct lt_cell);
  size_t i = 0;

  for (; i + 32 <= n; i += 32) {
    __m256i va = _mm256_loadu_si256((const __m256i *)(pa + i));
    __m256i vb = _mm256_loadu_si256((const __m256i *)(pb + i));
    uint32_t eq = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(va, vb));
    if (eq != 0xFFFFFFFF) {
      size_t byte_idx = i + (size_t)lt__ctz32(~eq);
      return (int)(byte_idx / sizeof(struct lt_cell));
    }
  }

  for (; i < n; i++)
    if (pa[i] != pb[i])
      return (int)(i / sizeof(struct lt_cell));

  return count;
}

/* AVX2 first-equal-cell: 16-byte cells x 2 per ymm. A cell is equal iff
 * its 16 movemask bits are all set. Relies on Step E's invariant that
 * `_reserved` is 0 in both buffers, which makes 16-byte byte-equality
 * equivalent to field-equality. */
int lt__simd_diff_first_equal_cell(const struct lt_cell *a,
                                   const struct lt_cell *b, int count) {
  if (count <= 0)
    return count;

  const char *pa = (const char *)a;
  const char *pb = (const char *)b;
  int pairs = count / 2;

  for (int p = 0; p < pairs; p++) {
    __m256i va = _mm256_loadu_si256((const __m256i *)(pa + p * 32));
    __m256i vb = _mm256_loadu_si256((const __m256i *)(pb + p * 32));
    uint32_t eq = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(va, vb));

    if ((eq & 0x0000FFFFu) == 0x0000FFFFu)
      return p * 2;
    if ((eq & 0xFFFF0000u) == 0xFFFF0000u)
      return p * 2 + 1;
  }

  int i = pairs * 2;
  if (i < count) {
    if (a[i].ch == b[i].ch && a[i].fg == b[i].fg && a[i].bg == b[i].bg)
      return i;
  }

  return count;
}

/* AVX2 fill: broadcast a 16-byte cell template into a ymm (= 2 cells)
 * and store 32 bytes per iteration. Tail handled scalar. */
void lt__simd_fill_cells(struct lt_cell *buf, int count, lt_attr fg,
                         lt_attr bg) {
  if (count <= 0)
    return;

  struct lt_cell tmpl = {
      .ch = ' ',
      .fg = fg,
      .bg = bg,
      ._reserved = 0,
  };
  __m128i x = _mm_loadu_si128((const __m128i *)&tmpl);
  __m256i v = _mm256_broadcastsi128_si256(x);

  char *p = (char *)buf;
  int pairs = count / 2;

  for (int i = 0; i < pairs; i++)
    _mm256_storeu_si256((__m256i *)(p + i * 32), v);

  int tail = pairs * 2;
  for (int i = tail; i < count; i++)
    buf[i] = tmpl;
}

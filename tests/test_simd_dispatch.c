/* Dispatcher sanity (dispatch builds only — registered conditionally in
 * tests/CMakeLists.txt): the startup constructor picked a compiled,
 * CPU-supported backend, and the canonical forwarders route to exactly that
 * backend. White-box: links libterm_static. */
#include "intrinsics/diff.h"
#include "intrinsics/dispatch.h"

#include <assert.h>
#include <string.h>

int main(void) {
  const char *active = lt__simd_active_backend();

  /* Active backend is one of the compiled set... */
  int known = 0;
#ifdef LT_SIMD_HAVE_SCALAR
  known |= strcmp(active, "scalar") == 0;
#endif
#ifdef LT_SIMD_HAVE_AVX2
  known |= strcmp(active, "avx2") == 0;
#endif
#ifdef LT_SIMD_HAVE_AVX512
  known |= strcmp(active, "avx512") == 0;
#endif
#ifdef LT_SIMD_HAVE_NEON
  known |= strcmp(active, "neon") == 0;
#endif
#ifdef LT_SIMD_HAVE_SVE
  known |= strcmp(active, "sve") == 0;
#endif
#ifdef LT_SIMD_HAVE_RVV
  known |= strcmp(active, "rvv") == 0;
#endif
  assert(known);

  /* ...and runnable on this CPU. */
  assert(lt__simd_backend_supported(active));

  /* Forwarders agree with the active backend called directly. */
  struct lt_cell a[17], b[17];
  for (int i = 0; i < 17; i++) {
    a[i].ch = 'x';
    a[i].fg = LT_WHITE;
    a[i].bg = LT_DEFAULT;
    a[i]._reserved = 0;
    b[i] = a[i];
  }
  b[9].fg = LT_RED;
  assert(lt__simd_diff_first_differ_cell(a, b, 17) == 9);

  lt__simd_diff_fn direct = 0;
#ifdef LT_SIMD_HAVE_SCALAR
  if (strcmp(active, "scalar") == 0)
    direct = lt__simd_diff_first_differ_cell_scalar;
#endif
#ifdef LT_SIMD_HAVE_AVX2
  if (strcmp(active, "avx2") == 0)
    direct = lt__simd_diff_first_differ_cell_avx2;
#endif
#ifdef LT_SIMD_HAVE_AVX512
  if (strcmp(active, "avx512") == 0)
    direct = lt__simd_diff_first_differ_cell_avx512;
#endif
#ifdef LT_SIMD_HAVE_NEON
  if (strcmp(active, "neon") == 0)
    direct = lt__simd_diff_first_differ_cell_neon;
#endif
#ifdef LT_SIMD_HAVE_SVE
  if (strcmp(active, "sve") == 0)
    direct = lt__simd_diff_first_differ_cell_sve;
#endif
#ifdef LT_SIMD_HAVE_RVV
  if (strcmp(active, "rvv") == 0)
    direct = lt__simd_diff_first_differ_cell_rvv;
#endif
  assert(direct != 0);
  assert(direct(a, b, 17) == 9);

  return 0;
}

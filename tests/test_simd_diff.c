#include "intrinsics/diff.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

#ifdef LT_SIMD_DISPATCH
#include "intrinsics/dispatch.h"

/* In dispatch builds the suite below runs once through the canonical
 * forwarders and then once per compiled backend the running CPU supports.
 * The function-like redirection keeps every test body byte-identical. */
static lt__simd_diff_fn g_differ = lt__simd_diff_first_differ_cell;
static lt__simd_diff_fn g_equal = lt__simd_diff_first_equal_cell;
static lt__simd_fill_fn g_fill = lt__simd_fill_cells;
#define lt__simd_diff_first_differ_cell g_differ
#define lt__simd_diff_first_equal_cell g_equal
#define lt__simd_fill_cells g_fill
#endif

/* Large enough that the untouched-region guard below has headroom past the
 * widest emulated vector (qemu SVE/RVV under -cpu max). */
#define MAXN 64

static void set_cell(struct lt_cell *c, lt_uchar ch, lt_attr fg, lt_attr bg) {
  c->ch = ch;
  c->fg = fg;
  c->bg = bg;
  c->_reserved = 0;
}

/* a[] and b[] all-equal 'x' cells. */
static void make_equal(struct lt_cell *a, struct lt_cell *b, int n) {
  for (int i = 0; i < n; i++) {
    set_cell(&a[i], 'x', LT_WHITE, LT_DEFAULT);
    set_cell(&b[i], 'x', LT_WHITE, LT_DEFAULT);
  }
}

/* a[] and b[] fully different cells (no field shared). */
static void make_diff(struct lt_cell *a, struct lt_cell *b, int n) {
  for (int i = 0; i < n; i++) {
    set_cell(&a[i], 'A', LT_WHITE, LT_DEFAULT);
    set_cell(&b[i], 'B', LT_RED, LT_BLUE);
  }
}

static void test_first_differ(int n) {
  struct lt_cell a[MAXN], b[MAXN];

  make_equal(a, b, n);
  assert(lt__simd_diff_first_differ_cell(a, b, n) == n);

  for (int k = 0; k < n; k++) {
    make_equal(a, b, n);
    b[k].fg = LT_RED;
    assert(lt__simd_diff_first_differ_cell(a, b, n) == k);
  }
}

static void test_first_equal(int n) {
  struct lt_cell a[MAXN], b[MAXN];

  make_diff(a, b, n);
  assert(lt__simd_diff_first_equal_cell(a, b, n) == n);

  for (int k = 0; k < n; k++) {
    make_diff(a, b, n);
    b[k] = a[k];
    assert(lt__simd_diff_first_equal_cell(a, b, n) == k);
  }
}

/* The cluster id lives in _reserved. Two cells with identical ch/fg/bg but
 * different cluster ids render differently (e.g. e+acute vs e+grave), so the
 * diff MUST treat them as unequal. This is the contract every backend's
 * scalar tail historically violated. Exercise every position so SIMD tails
 * are covered. */
static void test_reserved_distinguishes(int n) {
  struct lt_cell a[MAXN], b[MAXN];

  for (int k = 0; k < n; k++) {
    /* Identical everywhere... */
    make_equal(a, b, n);
    /* ...except cell k differs only in its cluster id. */
    b[k]._reserved = 7;

    /* first_differ must report k, not n (must not skip it as "equal"). */
    assert(lt__simd_diff_first_differ_cell(a, b, n) == k);

    /* In an otherwise all-different array, a cell matching in ch/fg/bg but
     * differing in _reserved must NOT count as the first equal cell. */
    make_diff(a, b, n);
    b[k] = a[k];
    b[k]._reserved = 7; /* same ch/fg/bg as a[k], different cluster */
    /* cell k is no longer "equal", so first-equal should look past it: either
     * another later equal cell or n. With make_diff, no other cell is equal. */
    assert(lt__simd_diff_first_equal_cell(a, b, n) == n);
  }
}

static void test_fill(int n) {
  struct lt_cell buf[MAXN];
  unsigned char ref[sizeof(struct lt_cell)];
  memset(ref, 0xAB, sizeof ref);
  memset(buf, 0xAB, sizeof buf);

  lt__simd_fill_cells(buf, n, LT_GREEN, LT_BLUE);

  for (int i = 0; i < n; i++) {
    assert(buf[i].ch == (lt_uchar)' ');
    assert(buf[i].fg == (lt_attr)LT_GREEN);
    assert(buf[i].bg == (lt_attr)LT_BLUE);
    assert(buf[i]._reserved == 0);
  }
  /* fill must not write past `n` cells (catches a vector store overrunning). */
  for (int i = n; i < MAXN; i++)
    assert(memcmp(&buf[i], ref, sizeof(struct lt_cell)) == 0);
}

static void run_suite(void) {
  const int counts[] = {0, 1, 2, 3, 4, 5, 8, 9, 16, 17};

  for (size_t c = 0; c < sizeof counts / sizeof counts[0]; c++) {
    test_first_differ(counts[c]);
    test_first_equal(counts[c]);
    test_reserved_distinguishes(counts[c]);
    test_fill(counts[c]);
  }

  return;
}

int main(void) {
  /* Canonical names (in dispatch builds: the active backend, via the
   * forwarders; in static builds: the single compiled backend). */
  run_suite();

#ifdef LT_SIMD_DISPATCH
/* And each compiled backend directly, skipping ones this CPU can't run. */
#define LT__RUN_BACKEND(suffix, name)                                          \
  do {                                                                         \
    if (lt__simd_backend_supported(name)) {                                    \
      g_differ = lt__simd_diff_first_differ_cell_##suffix;                     \
      g_equal = lt__simd_diff_first_equal_cell_##suffix;                       \
      g_fill = lt__simd_fill_cells_##suffix;                                   \
      run_suite();                                                             \
    }                                                                          \
  } while (0)
#ifdef LT_SIMD_HAVE_SCALAR
  LT__RUN_BACKEND(scalar, "scalar");
#endif
#ifdef LT_SIMD_HAVE_AVX2
  LT__RUN_BACKEND(avx2, "avx2");
#endif
#ifdef LT_SIMD_HAVE_AVX512
  LT__RUN_BACKEND(avx512, "avx512");
#endif
#ifdef LT_SIMD_HAVE_NEON
  LT__RUN_BACKEND(neon, "neon");
#endif
#ifdef LT_SIMD_HAVE_SVE
  LT__RUN_BACKEND(sve, "sve");
#endif
#ifdef LT_SIMD_HAVE_RVV
  LT__RUN_BACKEND(rvv, "rvv");
#endif
#endif /* LT_SIMD_DISPATCH */

  return 0;
}

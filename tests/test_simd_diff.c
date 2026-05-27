#include "intrinsics/diff.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

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

int main(void) {
  const int counts[] = {0, 1, 2, 3, 4, 5, 8, 9, 16, 17};

  for (size_t c = 0; c < sizeof counts / sizeof counts[0]; c++) {
    test_first_differ(counts[c]);
    test_first_equal(counts[c]);
    test_fill(counts[c]);
  }

  return 0;
}

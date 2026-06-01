/* Grapheme clusters: lt_set_cell_ex / lt_extend_cell store a base + trailing
 * codepoints in one cell, lt_get_cell round-trips the base + cluster id, and
 * lt_present emits the full cluster (base then continuation codepoints) as
 * UTF-8. Driven through an openpty() master for the render check. POSIX-only;
 * returns 77 (CTest "skip") when no pty is available. */
#define _DEFAULT_SOURCE
#include "internal.h"
#include "libterm/libterm.h"

#include <assert.h>
#include <fcntl.h>
#include <pty.h>
#include <string.h>
#include <unistd.h>

static char g_buf[65536];
static size_t g_len = 0;

static void drain(int master) {
  for (;;) {
    char tmp[4096];
    ssize_t n = read(master, tmp, sizeof tmp);
    if (n <= 0)
      break;
    if (g_len + (size_t)n > sizeof g_buf)
      n = (ssize_t)(sizeof g_buf - g_len);
    if (n <= 0)
      break;
    memcpy(g_buf + g_len, tmp, (size_t)n);
    g_len += (size_t)n;
  }
}

static int contains(const char *needle, size_t nlen) {
  if (nlen == 0 || g_len < nlen)
    return 0;
  for (size_t i = 0; i + nlen <= g_len; i++)
    if (memcmp(g_buf + i, needle, nlen) == 0)
      return 1;
  return 0;
}

int main(void) {
  assert(lt_has_egc() == 1);

  int master = -1, slave = -1;
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;

  if (openpty(&master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* CTest skip */
  int fl = fcntl(master, F_GETFL, 0);
  fcntl(master, F_SETFL, fl | O_NONBLOCK);

  assert(lt_init_fd(slave) == LT_OK);
  assert(lt_clear() == LT_OK);

  /* e + U+0301 (combining acute) as a cluster: base in ch, nonzero cluster id
   * in _reserved. */
  {
    const lt_uchar cl[] = {'e', 0x0301};
    assert(lt_set_cell_ex(0, 0, cl, 2, LT_DEFAULT, LT_DEFAULT) == LT_OK);
    struct lt_cell c;
    assert(lt_get_cell(0, 0, &c) == LT_OK);
    assert(c.ch == 'e');
    assert(c._reserved != 0);
  }

  /* Content dedup: the identical cluster written elsewhere gets the SAME id
   * (required so the present() diff treats it as unchanged). */
  {
    const lt_uchar cl[] = {'e', 0x0301};
    struct lt_cell a, b;
    assert(lt_get_cell(0, 0, &a) == LT_OK);
    assert(lt_set_cell_ex(5, 0, cl, 2, LT_DEFAULT, LT_DEFAULT) == LT_OK);
    assert(lt_get_cell(5, 0, &b) == LT_OK);
    assert(a._reserved == b._reserved);
  }

  /* A different cluster gets a different id. */
  {
    const lt_uchar cl[] = {'a', 0x0301};
    struct lt_cell a, b;
    assert(lt_get_cell(0, 0, &a) == LT_OK);
    assert(lt_set_cell_ex(6, 0, cl, 2, LT_DEFAULT, LT_DEFAULT) == LT_OK);
    assert(lt_get_cell(6, 0, &b) == LT_OK);
    assert(a._reserved != b._reserved);
  }

  /* nch == 1 behaves like lt_set_cell: no cluster id. */
  {
    const lt_uchar cl[] = {'Z'};
    assert(lt_set_cell_ex(1, 1, cl, 1, LT_DEFAULT, LT_DEFAULT) == LT_OK);
    struct lt_cell c;
    assert(lt_get_cell(1, 1, &c) == LT_OK);
    assert(c.ch == 'Z');
    assert(c._reserved == 0);
  }

  /* lt_extend_cell grows a plain cell into a cluster. */
  {
    assert(lt_set_cell(2, 2, 'o', LT_DEFAULT, LT_DEFAULT) == LT_OK);
    assert(lt_extend_cell(2, 2, 0x0308) == LT_OK); /* combining diaeresis */
    struct lt_cell c;
    assert(lt_get_cell(2, 2, &c) == LT_OK);
    assert(c.ch == 'o');
    assert(c._reserved != 0);
  }

  /* Render: the cluster cell emits base 'e' (0x65) then U+0301 (0xCC 0x81) —
   * and the base exactly ONCE. (A regression once emitted ch AND the cluster's
   * index-0 base, producing "ee\xCC\x81"; assert that doesn't happen.) */
  {
    g_len = 0;
    assert(lt_present() == LT_OK);
    drain(master);
    const char want[] = {'e', (char)0xCC, (char)0x81};
    assert(contains(want, sizeof want));
    const char doubled[] = {'e', 'e', (char)0xCC, (char)0x81};
    assert(!contains(doubled, sizeof doubled)); /* base must not be doubled */
  }

  /* Error cases. */
  {
    const lt_uchar cl[] = {'x', 0x0301};
    assert(lt_set_cell_ex(-1, 0, cl, 2, LT_DEFAULT, LT_DEFAULT) ==
           LT_ERR_OUT_OF_BOUNDS);
    assert(lt_set_cell_ex(0, 0, NULL, 2, LT_DEFAULT, LT_DEFAULT) == LT_ERR);
    assert(lt_set_cell_ex(0, 0, cl, 0, LT_DEFAULT, LT_DEFAULT) == LT_ERR);
  }

  assert(lt_shutdown() == LT_OK);

  /* After shutdown the table is reset: a fresh session re-interns from id 1. */
  assert(lt_init_fd(slave) == LT_OK);
  {
    const lt_uchar cl[] = {'q', 0x0301};
    assert(lt_clear() == LT_OK);
    assert(lt_set_cell_ex(0, 0, cl, 2, LT_DEFAULT, LT_DEFAULT) == LT_OK);
    struct lt_cell c;
    assert(lt_get_cell(0, 0, &c) == LT_OK);
    assert(c._reserved == 1); /* first cluster of the new session */
  }

  /* Bounded memory: write a long sequence of DISTINCT clusters into the same
   * cell, presenting between each so the previous cluster becomes unreferenced
   * (not in back, and overwritten in front by the present). Without slot reuse
   * the table would grow to ~count; with the liveness sweep the live id stays
   * small. We assert the id never runs away. */
  {
    assert(lt_clear() == LT_OK);
    assert(lt_present() == LT_OK);
    uint32_t max_id = 0;
    for (int i = 0; i < 2000; i++) {
      /* Distinct base each iteration so every cluster is unique. */
      lt_uchar cl[2] = {(lt_uchar)('A' + (i % 26)), 0x0301u + (lt_uchar)i};
      assert(lt_set_cell_ex(0, 0, cl, 2, LT_DEFAULT, LT_DEFAULT) == LT_OK);
      struct lt_cell c;
      assert(lt_get_cell(0, 0, &c) == LT_OK);
      if (c._reserved > max_id)
        max_id = c._reserved;
      assert(lt_present() == LT_OK); /* front now also holds this id */
      g_len = 0;
      drain(master); /* keep the pty buffer from filling and blocking writes */
    }
    /* 2000 distinct clusters written, but at most a couple are ever live at
     * once (the cell's current + the front copy), so ids stay tiny. A runaway
     * (no sweep) would push this toward ~2000. */
    assert(max_id < 64);
  }

  /* Stale-render guard: after a slot is recycled for a DIFFERENT cluster, the
   * next present must actually repaint that cell — it must not be fooled by a
   * matching _reserved byte into skipping. Verified by reading the emitted
   * bytes for the new cluster's continuation codepoint. */
  {
    assert(lt_clear() == LT_OK);
    assert(lt_present() == LT_OK);
    /* Churn to force sweeps/recycling, then land a known cluster. */
    for (int i = 0; i < 200; i++) {
      lt_uchar cl[2] = {'m', 0x0301u + (lt_uchar)i};
      assert(lt_set_cell_ex(3, 3, cl, 2, LT_DEFAULT, LT_DEFAULT) == LT_OK);
      assert(lt_present() == LT_OK);
      g_len = 0;
      drain(master);
    }
    /* Final cluster: 'm' + U+0308 (diaeresis = 0xCC 0x88). */
    const lt_uchar fin[] = {'m', 0x0308};
    assert(lt_set_cell_ex(3, 3, fin, 2, LT_DEFAULT, LT_DEFAULT) == LT_OK);
    g_len = 0;
    assert(lt_present() == LT_OK);
    drain(master);
    const char want[] = {'m', (char)0xCC, (char)0x88};
    assert(contains(want, sizeof want));
  }

  assert(lt_shutdown() == LT_OK);

  close(slave);
  close(master);
  return 0;
}

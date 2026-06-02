/*
 * libterm - grapheme-cluster (EGC) table.
 *
 * A cell stores a single codepoint in `ch` plus a cluster id in `_reserved`
 * (0 = none). Multi-codepoint clusters (a base plus combining marks, or a ZWJ
 * emoji sequence) live here, out of line, so struct lt_cell stays 16 bytes and
 * the SIMD diff keeps working on raw bytes.
 *
 * Clusters are interned with content dedup: identical clusters get the same id.
 * That is not just to bound the table — it is required for correctness. The
 * present() diff treats back == front (byte-equality, including _reserved) as
 * "unchanged". If the same cluster were given a fresh id each frame it would
 * never byte-match the front buffer and would repaint forever. Dedup makes
 * "equal id <=> equal cluster", so the invariant holds.
 *
 * Memory is bounded by the number of *simultaneously live* distinct clusters,
 * not by session history: when the table would otherwise grow, a liveness
 * sweep frees clusters that no cell references and reuses their slots.
 *
 * Sweep correctness — why BOTH buffers are scanned: present() only ever
 * dereferences back's cluster ids, but it byte-compares front's _reserved
 * against back's. If a slot referenced only by front were freed and then reused
 * for a different cluster, a later back cell reusing that id would byte-match
 * the stale front id and present() would wrongly skip the repaint. So an id is
 * "live" if EITHER back or front references it.
 */
#include "internal.h"
#include <stdlib.h>
#include <string.h>

struct lt__egc_entry {
  lt_uchar *cps; /* owned: nch codepoints, base first; NULL = free slot */
  size_t nch;
};

/* entries[0] is unused so that id 0 can mean "no cluster". lt__egc_count is the
 * high-water mark: slots [1, count) have been allocated at least once (a slot
 * with cps == NULL inside that range is free for reuse). */
static struct lt__egc_entry *lt__egc_entries = NULL;
static size_t lt__egc_count = 0;
static size_t lt__egc_cap = 0;

static bool lt__egc_same(const struct lt__egc_entry *e, const lt_uchar *chs,
                         size_t nch) {
  if (e->nch != nch)
    return false;
  for (size_t i = 0; i < nch; i++)
    if (e->cps[i] != chs[i])
      return false;
  return true;
}

/* Free every cluster slot not referenced by the back or front buffer, marking
 * it reusable. No-op if the buffers aren't available yet. See the file header
 * for why both buffers must be scanned. */
static void lt__egc_sweep(void) {
  if (lt__egc_count <= 1)
    return;

  bool *live = calloc(lt__egc_count, sizeof(bool));
  if (!live)
    return; /* can't sweep this round; correctness is unaffected, only memory */

  /* Cast before multiplying: int w * int h can overflow INT_MAX (UB). */
  const size_t n = (size_t)lt__g.width * (size_t)lt__g.height;
  const struct lt_cell *bufs[2] = {lt__g.back, lt__g.front};
  for (int b = 0; b < 2; b++) {
    const struct lt_cell *buf = bufs[b];
    if (!buf)
      continue;
    for (size_t i = 0; i < n; i++) {
      uint32_t id = buf[i]._reserved;
      if (id > 0 && id < lt__egc_count)
        live[id] = true;
    }
  }

  for (size_t id = 1; id < lt__egc_count; id++) {
    if (!live[id] && lt__egc_entries[id].cps) {
      free(lt__egc_entries[id].cps);
      lt__egc_entries[id].cps = NULL;
      lt__egc_entries[id].nch = 0;
    }
  }

  free(live);
}

uint32_t lt__egc_intern(const lt_uchar *chs, size_t nch, bool *ok) {
  if (ok)
    *ok = true;

  /* A lone codepoint is not a cluster; the caller stores it in ch directly. */
  if (!chs || nch <= 1)
    return 0;

  /* Single pass over the table: return an existing identical cluster (dedup),
   * and remember the first free slot in case we need to allocate. */
  uint32_t free_slot = 0;
  for (size_t id = 1; id < lt__egc_count; id++) {
    if (!lt__egc_entries[id].cps) {
      if (!free_slot)
        free_slot = (uint32_t)id;
      continue;
    }
    if (lt__egc_same(&lt__egc_entries[id], chs, nch))
      return (uint32_t)id;
  }

  /* Reserve slot 0 as the dead "no cluster" id on first use. */
  if (lt__egc_count == 0)
    lt__egc_count = 1;

  /* No free slot? Try a liveness sweep before growing the table. */
  if (free_slot == 0) {
    lt__egc_sweep();
    for (size_t id = 1; id < lt__egc_count; id++)
      if (!lt__egc_entries[id].cps) {
        free_slot = (uint32_t)id;
        break;
      }
  }

  lt_uchar *cps = malloc(nch * sizeof(*cps));
  if (!cps) {
    if (ok)
      *ok = false;
    return 0;
  }
  memcpy(cps, chs, nch * sizeof(*cps));

  uint32_t id;
  if (free_slot != 0) {
    id = free_slot; /* reuse a swept slot */
  } else {
    if (lt__egc_count >= lt__egc_cap) {
      size_t ncap = lt__egc_cap ? lt__egc_cap * 2 : 16;
      struct lt__egc_entry *grown =
          realloc(lt__egc_entries, ncap * sizeof(*grown));
      if (!grown) {
        free(cps);
        if (ok)
          *ok = false;
        return 0;
      }
      lt__egc_entries = grown;
      lt__egc_cap = ncap;
    }
    id = (uint32_t)lt__egc_count++;
  }

  lt__egc_entries[id].cps = cps;
  lt__egc_entries[id].nch = nch;
  return id;
}

const lt_uchar *lt__egc_get(uint32_t id, size_t *nch) {
  if (id == 0 || id >= lt__egc_count || !lt__egc_entries[id].cps)
    return NULL;
  if (nch)
    *nch = lt__egc_entries[id].nch;
  return lt__egc_entries[id].cps;
}

void lt__egc_reset(void) {
  for (size_t i = 1; i < lt__egc_count; i++)
    free(lt__egc_entries[i].cps); /* free(NULL) on swept slots is fine */
  free(lt__egc_entries);
  lt__egc_entries = NULL;
  lt__egc_count = 0;
  lt__egc_cap = 0;
}

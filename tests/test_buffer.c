#include "libterm/libterm.h"
#include <assert.h>

int main(void) {
  int rc = lt_init();
  if (rc == LT_ERR_INIT_OPEN)
    return 0;
  assert(rc == LT_OK);

  int w = lt_width();
  int h = lt_height();
  assert(w > 0);
  assert(h > 0);

  assert(lt_set_clear_attrs(LT_WHITE, LT_BLACK) == LT_OK);
  assert(lt_clear() == LT_OK);

  /* in-bounds writes */
  assert(lt_set_cell(0, 0, (lt_uchar)'A', LT_WHITE, LT_BLACK) == LT_OK);
  assert(lt_set_cell(w - 1, h - 1, (lt_uchar)'Z', LT_GREEN, LT_BLACK) == LT_OK);

  /* out-of-bounds writes */
  assert(lt_set_cell(-1, 0, (lt_uchar)'x', LT_DEFAULT, LT_DEFAULT) ==
         LT_ERR_OUT_OF_BOUNDS);
  assert(lt_set_cell(0, -1, (lt_uchar)'x', LT_DEFAULT, LT_DEFAULT) ==
         LT_ERR_OUT_OF_BOUNDS);
  assert(lt_set_cell(w, 0, (lt_uchar)'x', LT_DEFAULT, LT_DEFAULT) ==
         LT_ERR_OUT_OF_BOUNDS);
  assert(lt_set_cell(0, h, (lt_uchar)'x', LT_DEFAULT, LT_DEFAULT) ==
         LT_ERR_OUT_OF_BOUNDS);

  /* present path and idempotent no-change present */
  assert(lt_present() == LT_OK);
  assert(lt_present() == LT_OK);

  /* rewrite same cell multiple times should stay valid */
  assert(lt_set_cell(1, 1, (lt_uchar)'1', LT_WHITE, LT_BLACK) == LT_OK);
  assert(lt_set_cell(1, 1, (lt_uchar)'2', LT_WHITE, LT_BLACK) == LT_OK);
  assert(lt_set_cell(1, 1, (lt_uchar)'3', LT_WHITE, LT_BLACK) == LT_OK);
  assert(lt_present() == LT_OK);

  assert(lt_clear() == LT_OK);
  assert(lt_present() == LT_OK);

  assert(lt_shutdown() == LT_OK);
  return 0;
}

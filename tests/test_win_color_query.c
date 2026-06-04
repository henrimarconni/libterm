/* Windows color-query mapping test: drives the pure CSBI mapper with a
 * fabricated CONSOLE_SCREEN_BUFFER_INFOEX (no console needed), asserting the
 * attribute-nibble selection and the COLORREF (0x00BBGGRR) -> 0x00RRGGBB
 * swizzle. White-box: links libterm_static and calls lt__ internals (pattern
 * of test_win_sgr_output). */
#include "internal.h"
#include "libterm/libterm.h"
#include "platform/windows/win_internal.h"
#include <assert.h>
#include <string.h>

int main(void) {
  CONSOLE_SCREEN_BUFFER_INFOEX csbi;
  memset(&csbi, 0, sizeof(csbi));
  csbi.cbSize = sizeof(csbi);
  csbi.wAttributes = (1 << 4) | 7; /* fg nibble = 7, bg nibble = 1 */
  csbi.ColorTable[7] = RGB(0xC0, 0xC7, 0xC8);
  csbi.ColorTable[1] = RGB(0x1C, 0x1D, 0x1E);
  csbi.ColorTable[4] = RGB(0xFF, 0x80, 0x00);

  uint32_t rgb = 0;
  assert(lt__win_query_color_from_csbi(&csbi, LT_COLOR_DEFAULT_FG, &rgb) ==
         LT_OK);
  assert(rgb == 0xC0C7C8);
  assert(lt__win_query_color_from_csbi(&csbi, LT_COLOR_DEFAULT_BG, &rgb) ==
         LT_OK);
  assert(rgb == 0x1C1D1E);
  assert(lt__win_query_color_from_csbi(&csbi, 4, &rgb) == LT_OK);
  assert(rgb == 0xFF8000);
  assert(lt__win_query_color_from_csbi(&csbi, 16, &rgb) ==
         LT_ERR_UNSUPPORTED_TERM);
  assert(lt__win_query_color_from_csbi(&csbi, 255, &rgb) ==
         LT_ERR_UNSUPPORTED_TERM);
  return 0;
}

#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#include <stdlib.h>

/* Portable env helpers: POSIX setenv/unsetenv are unavailable in the MinGW
 * CRT, so map onto _putenv_s on Windows. Setting an empty value removes the
 * variable there, which matches unsetenv for lt_detect_color_depth's getenv
 * checks. */
#ifdef _WIN32
static void set_env(const char *name, const char *value) {
  _putenv_s(name, value);
}
static void unset_env(const char *name) { _putenv_s(name, ""); }
#else
static void set_env(const char *name, const char *value) {
  setenv(name, value, 1);
}
static void unset_env(const char *name) { unsetenv(name); }
#endif

int main(void) {
  unset_env("TERM");
  set_env("COLORTERM", "truecolor");
  assert(lt_detect_color_depth() == LT_OUTPUT_TRUECOLOR);

  set_env("COLORTERM", "24bit");
  assert(lt_detect_color_depth() == LT_OUTPUT_TRUECOLOR);

  set_env("COLORTERM", "gnome-terminal");
  set_env("TERM", "xterm-256color");
  assert(lt_detect_color_depth() == LT_OUTPUT_256);

  unset_env("COLORTERM");
  set_env("TERM", "screen-256color");
  assert(lt_detect_color_depth() == LT_OUTPUT_256);

  set_env("TERM", "xterm");
  assert(lt_detect_color_depth() == LT_OUTPUT_NORMAL);

  set_env("TERM", "dumb");
  assert(lt_detect_color_depth() == LT_OUTPUT_NORMAL);

  unset_env("COLORTERM");
  unset_env("TERM");
  assert(lt_detect_color_depth() == LT_OUTPUT_NORMAL);

  return 0;
}

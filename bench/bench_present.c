#if defined(_WIN32)
#include <profileapi.h>
#include <windows.h>
#else
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#endif

#include "internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef LIBTERM_BENCH_WIDTH
#define LIBTERM_BENCH_WIDTH 40
#endif

#ifndef LIBTERM_BENCH_HEIGHT
#define LIBTERM_BENCH_HEIGHT 12
#endif

typedef void (*workload_fn)(int frame_index);

struct workload_case {
  const char *name;
  workload_fn fn;
};

static volatile uint64_t g_sink = 0;
static void workload_baseline(int frame_index) {
  g_sink += (uint64_t)(frame_index & 1);
}

static void workload_sparse(int frame_index) {
  int x0 = frame_index % LIBTERM_BENCH_WIDTH;
  int y0 = frame_index % LIBTERM_BENCH_HEIGHT;

  lt_set_cell(x0, y0, 'A', LT_DEFAULT, LT_DEFAULT);
  lt_set_cell((x0 + 1) % LIBTERM_BENCH_WIDTH, y0, 'B', LT_DEFAULT, LT_DEFAULT);
  lt_set_cell(x0, (y0 + 1) % LIBTERM_BENCH_HEIGHT, 'C', LT_DEFAULT, LT_DEFAULT);
  lt_set_cell((x0 + 1) % LIBTERM_BENCH_WIDTH, (y0 + 1) % LIBTERM_BENCH_HEIGHT,
              'D', LT_DEFAULT, LT_DEFAULT);

  lt_present();

  g_sink += (uint64_t)(frame_index & 1);
}

static void workload_full(int frame_index) {
  const int w = LIBTERM_BENCH_WIDTH, h = LIBTERM_BENCH_HEIGHT;
  char ch = (char)('A' + (frame_index & 15));

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      lt_set_cell(x, y, ch, LT_DEFAULT, LT_DEFAULT);
    }
  }

  lt_present();
  g_sink += (uint64_t)(frame_index & 1);
}

static void workload_box_redraw(int frame_index) {
  const int w = LIBTERM_BENCH_WIDTH, h = LIBTERM_BENCH_HEIGHT;

  for (int x = 0; x < w; x++) {
    lt_set_cell(x, 0, '-', LT_DEFAULT, LT_DEFAULT);
    lt_set_cell(x, h - 1, '-', LT_DEFAULT, LT_DEFAULT);
  }

  /* left + right */
  for (int y = 1; y < h - 1; y++) {
    lt_set_cell(0, y, '|', LT_DEFAULT, LT_DEFAULT);
    lt_set_cell(w - 1, y, '|', LT_DEFAULT, LT_DEFAULT);
  }

  /* corners */
  lt_set_cell(0, 0, '+', LT_DEFAULT, LT_DEFAULT);
  lt_set_cell(w - 1, 0, '+', LT_DEFAULT, LT_DEFAULT);
  lt_set_cell(0, h - 1, '+', LT_DEFAULT, LT_DEFAULT);
  lt_set_cell(w - 1, h - 1, '+', LT_DEFAULT, LT_DEFAULT);

  int tx = 2 + (frame_index % (w - 4));
  lt_set_cell(tx, 0, '*', LT_DEFAULT, LT_DEFAULT);

  lt_present();
  g_sink += (uint64_t)(frame_index & 1);
}

static void workload_sgr_rainbow(int frame_index) {
  const int w = LIBTERM_BENCH_WIDTH, h = LIBTERM_BENCH_HEIGHT;
  char ch = (char)('A' + (frame_index & 15));

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      /* Adjacent columns differ, so every cell starts a fresh SGR run.
       * Shifting by frame_index animates the palette so the colors never
       * match the previous present and emit_sgr fires every frame. */
      lt_attr fg = (lt_attr)(LT_BLACK + ((x + y + frame_index) % 8));
      lt_set_cell(x, y, ch, fg, LT_DEFAULT);
    }
  }

  lt_present();
  g_sink += (uint64_t)(frame_index & 1);
}

static void workload_sgr_attrs(int frame_index) {
  const int w = LIBTERM_BENCH_WIDTH, h = LIBTERM_BENCH_HEIGHT;
  char ch = (char)('A' + (frame_index & 15));

  /* Each step both adds and drops attribute bits, driving emit_sgr down its
   * reset-and-reapply path (cur_attrs carries bits the target lacks). */
  static const lt_attr attr_cycle[] = {
      0,
      LT_BOLD,
      LT_BOLD | LT_UNDERLINE,
      LT_UNDERLINE,
      LT_REVERSE,
      LT_ITALIC | LT_DIM,
  };
  const int n = (int)(sizeof(attr_cycle) / sizeof(attr_cycle[0]));

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      lt_attr attrs = attr_cycle[(x + frame_index) % n];
      lt_set_cell(x, y, ch, LT_WHITE | attrs, LT_DEFAULT);
    }
  }

  lt_present();
  g_sink += (uint64_t)(frame_index & 1);
}

static void workload_sgr_truecolor(int frame_index) {
  const int w = LIBTERM_BENCH_WIDTH, h = LIBTERM_BENCH_HEIGHT;
  char ch = (char)('A' + (frame_index & 15));

  /* Truecolor: animate distinct 24-bit fg AND bg per cell so every cell starts
   * a fresh SGR run and re-emits each frame, driving write_uint up to 6x/cell
   * (38;2;R;G;B for fg + 48;2;R;G;B for bg). */
  lt_set_output_mode(LT_OUTPUT_TRUECOLOR);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      lt_attr fg = LT_RGB((x + frame_index) & 0xFF, (y + frame_index) & 0xFF,
                          (x + y + frame_index) & 0xFF);
      lt_attr bg =
          LT_RGB((y + frame_index) & 0xFF, (x + y + frame_index) & 0xFF,
                 (x + frame_index) & 0xFF);
      lt_set_cell(x, y, ch, fg, bg);
    }
  }

  lt_present();
  g_sink += (uint64_t)(frame_index & 1);
}

static void workload_sgr_256(int frame_index) {
  const int w = LIBTERM_BENCH_WIDTH, h = LIBTERM_BENCH_HEIGHT;
  char ch = (char)('A' + (frame_index & 15));

  /* 256-palette: animate fg AND bg indices in 1..255 (avoid 0 = default) so
   * every cell re-emits and drives write_uint twice per cell (38;5;i + 48;5;i).
   */
  lt_set_output_mode(LT_OUTPUT_256);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      lt_attr fg = (lt_attr)(((x + y + frame_index) % 255) + 1);
      lt_attr bg = (lt_attr)(((x * 2 + y + frame_index) % 255) + 1);
      lt_set_cell(x, y, ch, fg, bg);
    }
  }

  lt_present();
  g_sink += (uint64_t)(frame_index & 1);
}

static uint64_t now_ns(void) {
#if defined(_WIN32)
  LARGE_INTEGER freq;
  LARGE_INTEGER counter;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&counter);
  return (uint64_t)counter.QuadPart * 1000000000ull / freq.QuadPart;
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

static int cmp_u64(const void *a, const void *b) {
  uint64_t ua = *(const uint64_t *)a;
  uint64_t ub = *(const uint64_t *)b;
  if (ua < ub)
    return -1;
  if (ua > ub)
    return 1;
  return 0;
}

static const struct workload_case k_workloads[] = {
    {"baseline", workload_baseline},
    {"sparse", workload_sparse},
    {"full", workload_full},
    {"box_redraw", workload_box_redraw},
    {"sgr_rainbow", workload_sgr_rainbow},
    {"sgr_attrs", workload_sgr_attrs},
    {"sgr_tcolor", workload_sgr_truecolor},
    {"sgr_256", workload_sgr_256},
};

#define k_workloads_count (sizeof(k_workloads) / sizeof(k_workloads[0]))

struct case_result {
  const char *name;
  double us_per_frame;
  double fps;
  double p50_us;
  double p95_us;
  struct lt__render_stats stats;
};

static void print_usage(const char *program) {
  fprintf(stderr, "usage: %s [all", program);
  for (int i = 0; i < (int)k_workloads_count; i++)
    fprintf(stderr, "|%s", k_workloads[i].name);
  fprintf(stderr, "]\n");
}

static int select_workload_range(int argc, char *argv[], int *start, int *end) {
  int count = k_workloads_count;

  *start = 0;
  *end = count;

  if (argc < 2 || strcmp(argv[1], "all") == 0)
    return 0;

  for (int i = 0; i < count; i++) {
    if (strcmp(argv[1], k_workloads[i].name) == 0) {
      *start = i;
      *end = i + 1;
      return 0;
    }
  }

  print_usage(argv[0]);
  return 2;
}

int main(int argc, char *argv[]) {

  const int warmup_frames = 20000;
  const int measure_frames = 10000;
  const int sample_frames = 2000;

  int start = 0, end = 0;
  int select_rc = select_workload_range(argc, argv, &start, &end);
  if (select_rc != 0)
    return select_rc;

  uint64_t frame_ns[2000];

  int rc = lt_init();
  if (rc != 0) {
    fprintf(stderr, "lt_init failed (rc=%d)\n", rc);
    return 1;
  }

  struct workload_case active = {0};
  struct case_result results[k_workloads_count];

  for (int i = start; i < end; i++) {
    active = k_workloads[i];
    for (int i = 0; i < warmup_frames; i++) {
      active.fn(i);
    }

    lt__g.stats = (struct lt__render_stats){0};

    uint64_t t0 = now_ns();
    for (int i = 0; i < measure_frames; i++) {
      uint64_t f0 = now_ns();
      active.fn(i);
      uint64_t f1 = now_ns();
      if (i < sample_frames)
        frame_ns[i] = f1 - f0;
    }
    uint64_t t1 = now_ns();

    double total_ms = (double)(t1 - t0) / 1000000.0;
    double us_per_frame = (double)(t1 - t0) / (double)measure_frames / 1000.0;
    double fps = (double)measure_frames / ((double)(t1 - t0) / 1000000000.0);

    qsort(frame_ns, (size_t)sample_frames, sizeof(frame_ns[0]), cmp_u64);
    double p50_us = (double)frame_ns[(int)(sample_frames * 50 / 100)] / 1000.0;
    double p95_us = (double)frame_ns[(int)(sample_frames * 95 / 100)] / 1000.0;
    results[i].name = active.name;
    results[i].us_per_frame = us_per_frame;
    results[i].fps = fps;
    results[i].p50_us = p50_us;
    results[i].p95_us = p95_us;
    results[i].stats = lt__g.stats;
  }

  lt_shutdown();

  printf("%-12s %12s %12s %10s %10s %10s %10s %10s %10s %10s %10s\n", "name",
         "us_per_frame", "fps", "p50_us", "p95_us", "runs", "cells", "moves",
         "bytes", "flushes", "sink");

  for (int i = start; i < end; i++) {
    printf("%-12s %12.3f %12.1f %10.3f %10.3f %10llu %10llu %10llu %10llu "
           "%10llu %10llu\n",
           results[i].name, results[i].us_per_frame, results[i].fps,
           results[i].p50_us, results[i].p95_us,
           (unsigned long long)results[i].stats.diff_runs,
           (unsigned long long)results[i].stats.cells_rendered,
           (unsigned long long)results[i].stats.cursor_moves,
           (unsigned long long)results[i].stats.bytes_buffered,
           (unsigned long long)results[i].stats.flushes,
           (unsigned long long)g_sink);
  }
  return 0;
}

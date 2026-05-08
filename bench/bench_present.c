#include "libterm/libterm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if defined(_WIN32)
#include <profileapi.h>
#include <windows.h>
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
  int x0 = frame_index % 40;
  int y0 = frame_index % 12;

  lt_set_cell(x0, y0, 'A', LT_DEFAULT, LT_DEFAULT);
  lt_set_cell((x0 + 1) % 40, y0, 'B', LT_DEFAULT, LT_DEFAULT);
  lt_set_cell(x0, (y0 + 1) % 12, 'C', LT_DEFAULT, LT_DEFAULT);
  lt_set_cell((x0 + 1) % 40, (y0 + 1) % 12, 'D', LT_DEFAULT, LT_DEFAULT);

  lt_present();

  g_sink += (uint64_t)(frame_index & 1);
}

static void workload_full(int frame_index) { lt_present(); }

static void workload_box_redraw(int frame_index) {
  const int w = 40, h = 12;

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
};

struct case_result {
  const char *name;
  double us_per_frame;
  double fps;
  double p50_us;
  double p95_us;
};

int main() {
  const int warmup_frames = 20000;
  const int measure_frames = 10000;
  const int sample_frames = 2000;
  uint64_t frame_ns[2000];
  int k_workloads_count = sizeof(k_workloads) / sizeof(k_workloads[0]);

  int rc = lt_init();
  if (rc != 0) {
    fprintf(stderr, "lt_init failed (rc=%d)\n", rc);
    return 1;
  }

  struct workload_case active = {0};
  struct case_result results[k_workloads_count];

  double biggest_upf = 0.0f;

  for (int i = 0; i < k_workloads_count; i++) {
    active = k_workloads[i];
    for (int i = 0; i < warmup_frames; i++) {
      active.fn(i);
    }

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

    double limit = us_per_frame * 1.15;
    if (limit < biggest_upf)
      biggest_upf = limit;

    if (p95_us < biggest_upf) {
      fprintf(
          stderr,
          "fail: workload=%s us_per_frame=%.3f, biggest_upf=%.3f p95_us=%.3f\n",
          active.name, us_per_frame, biggest_upf, p95_us);
      return 1;
    }
  }

  lt_shutdown();

  printf("%-12s %12s %12s %10s %10s %10s\n", "name", "us_per_frame", "fps",
         "p50_us", "p95_us", "sink");

  for (int i = 0; i < k_workloads_count; i++) {
    printf("%-12s %12.3f %12.1f %10.3f %10.3f %10llu\n", results[i].name,
           results[i].us_per_frame, results[i].fps, results[i].p50_us,
           results[i].p95_us, (unsigned long long)g_sink);
  }
  return 0;
}

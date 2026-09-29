#pragma once

#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#ifndef BENCH_FLAGS
#define BENCH_FLAGS "unknown"
#endif

#define MAX_SYSTEMS 32

#define MIN_RUNS  10
#define MAX_RUNS  100
#define CI_TARGET 0.05
#define Z_95      1.96

static const size_t work_levels[] = { 0, 10, 500 };
static const char* work_names[]   = { "none", "light", "heavy" };
#define WORK_LEVEL_COUNT 3

typedef struct
{
    double value;
} value_t;

typedef struct
{
    double median;
    double lo;
    double hi;
} stat_t;

typedef struct bench_s bench_t;

typedef struct
{
    const char* name;
    const char* lib;
    const char* variants[2];
    bool serial_only;
    double (*run_once)(bench_t* b, int thread_count); // returns the timed ms, setup excluded
    void (*cleanup)(bench_t* b);
} bench_def_t;

struct bench_s
{
    const bench_def_t* def;
    int system_count;
    size_t entity_count; // pro system
    size_t work_iterations;
    int variant;
};

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

static inline double bench_work(double acc, size_t iterations)
{
    for (size_t j = 0; j < iterations; j++)
        acc = acc * 0.999999 + 1.0;

    return acc;
}

static int compare_double(const void* a, const void* b)
{
    double x = *(const double*)a;
    double y = *(const double*)b;
    return (x > y) - (x < y);
}

static stat_t summarize(double* samples, int n)
{
    qsort(samples, n, sizeof(double), compare_double);

    double spread = Z_95 * sqrt(n);
    int lo        = (int)floor((n - spread) / 2);
    int hi        = (int)ceil(1 + (n + spread) / 2);

    if (lo < 1)
        lo = 1;
    if (hi > n)
        hi = n;

    stat_t s;
    s.median = n % 2 ? samples[n / 2] : (samples[n / 2 - 1] + samples[n / 2]) / 2;
    s.lo     = samples[lo - 1];
    s.hi     = samples[hi - 1];
    return s;
}

static double ci_width(stat_t s)
{
    return fmax(s.median - s.lo, s.hi - s.median) / s.median;
}

// TODO: one warm up run maybe useless
static stat_t measure(bench_t* b, int thread_count)
{
    double samples[MAX_RUNS];
    stat_t s = { 0 };

    b->def->run_once(b, thread_count);

    for (int n = 1; n <= MAX_RUNS; n++)
    {
        samples[n - 1] = b->def->run_once(b, thread_count);
        s              = summarize(samples, n);

        if (n >= MIN_RUNS && ci_width(s) <= CI_TARGET)
            break;
    }

    return s;
}

static void print_environment(int threads)
{
    char cpu[128] = "unknown";
#ifdef __APPLE__
    size_t size = sizeof(cpu);
    sysctlbyname("machdep.cpu.brand_string", cpu, &size, NULL, 0);
#else
    FILE* f = fopen("/proc/cpuinfo", "r");
    char line[256];
    while (f && fgets(line, sizeof(line), f))
        if (sscanf(line, "model name : %127[^\n]", cpu) == 1)
            break;
    if (f)
        fclose(f);
#endif

    struct utsname os;
    uname(&os);

    printf("cpu: %s (%d hardware threads), os: %s %s %s, compiler: %s, flags: %s\n",
           cpu,
           threads,
           os.sysname,
           os.release,
           os.machine,
           __VERSION__,
           BENCH_FLAGS);
}

static void baseline_path(char* path, size_t size, const bench_t* b)
{
    snprintf(path, size, "pico_%s.baseline", b->def->name);
}

static bool baseline_read(stat_t* pico, const bench_t* b)
{
    char path[64];
    baseline_path(path, sizeof(path), b);

    FILE* f = fopen(path, "r");
    if (!f)
        return false;

    int systems;
    size_t entities;
    bool ok = 2 == fscanf(f, "%d %zu", &systems, &entities) && systems == b->system_count &&
              entities == b->entity_count;

    for (int w = 0; ok && w < WORK_LEVEL_COUNT; w++)
        ok = 3 == fscanf(f, "%lf %lf %lf", &pico[w].median, &pico[w].lo, &pico[w].hi);

    fclose(f);
    return ok;
}

static void baseline_write(const stat_t* pico, const bench_t* b)
{
    char path[64];
    baseline_path(path, sizeof(path), b);

    FILE* f = fopen(path, "w");
    if (!f)
        return;

    fprintf(f, "%d %zu\n", b->system_count, b->entity_count);
    for (int w = 0; w < WORK_LEVEL_COUNT; w++)
        fprintf(f, "%f %f %f\n", pico[w].median, pico[w].lo, pico[w].hi);
    fclose(f);
}

static void measure_row(bench_t* b, int variant, int threads, stat_t* stats)
{
    b->variant = variant;

    for (int w = 0; w < WORK_LEVEL_COUNT; w++)
    {
        b->work_iterations = work_levels[w];
        stats[w]           = measure(b, threads);
    }
}

static void print_row(
    const bench_def_t* def, int variant, int threads, const stat_t* stats, const stat_t* pico)
{
    printf("%-6s %-26s %7d", def->lib, def->variants[variant], threads);

    for (int w = 0; w < WORK_LEVEL_COUNT; w++)
    {
        printf(" %10.3f +-%3.0f%%", stats[w].median, 100 * ci_width(stats[w]));

        if (pico)
        {
            bool overlap = stats[w].lo <= pico[w].hi && pico[w].lo <= stats[w].hi;
            printf(" %6.2fx%c", pico[w].median / stats[w].median, overlap ? '~' : ' ');
        }
        else
            printf(" %8s", "");
    }

    printf("\n");
    fflush(stdout);
}

static int bench_main(int argc, char** argv, const bench_def_t* def)
{
    bench_t b = { .def = def };

    b.entity_count = argc > 1 ? strtoul(argv[1], NULL, 10) : 100000;

    // Thread counts 1 - max_threads, in 2^ steps
    int hardware_threads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    int max_threads      = 1;
    while (max_threads * 2 <= hardware_threads && max_threads * 2 <= MAX_SYSTEMS)
        max_threads *= 2;

    b.system_count = max_threads;

    bool is_pico = strcmp(def->lib, "pico") == 0;

    stat_t pico[WORK_LEVEL_COUNT];
    bool has_pico = !is_pico && baseline_read(pico, &b);

    print_environment(hardware_threads);
    printf("%s (%s): %d systems x %zu entities, median ms +- 95%% CI, %d-%d runs\n",
           def->name,
           def->lib,
           b.system_count,
           b.entity_count,
           MIN_RUNS,
           MAX_RUNS);
    printf("work iterations per entity:");
    for (int w = 0; w < WORK_LEVEL_COUNT; w++)
        printf(" %s = %zu", work_names[w], work_levels[w]);
    printf("; ~ = CI overlaps pico's, the difference is not significant\n");

    printf("%-6s %-26s %7s", "lib", "variant", "threads");
    for (int w = 0; w < WORK_LEVEL_COUNT; w++)
        printf(" %17s %8s", work_names[w], "vs pico");
    printf("\n");

    int last_threads = def->serial_only ? 1 : max_threads;

    stat_t stats[WORK_LEVEL_COUNT];

    for (int v = 0; v < 2 && def->variants[v]; v++)
    {
        for (int threads = 1; threads <= last_threads; threads *= 2)
        {
            measure_row(&b, v, threads, stats);
            print_row(def, v, threads, stats, has_pico ? pico : NULL);
        }
    }

    // pico has 1 row
    if (is_pico)
        baseline_write(stats, &b);

    def->cleanup(&b);
    return 0;
}

#pragma once

#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#ifndef BENCH_FLAGS
#define BENCH_FLAGS "unknown"
#endif

#ifndef BENCH_RESULTS_DIR
#define BENCH_RESULTS_DIR "results"
#endif

#define MAX_SYSTEMS 32
#define MAX_ROWS    6

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

typedef struct
{
    int rows;
    int threads[MAX_ROWS];
    stat_t stats[MAX_ROWS][WORK_LEVEL_COUNT];
} baseline_t;

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
    FILE* csv;
    char csv_prefix[1024];
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

        if (b->csv)
            fprintf(b->csv,
                    "%s,%s,%s,%s,%d,%zu,%d,%.6f\n",
                    b->csv_prefix,
                    b->def->lib,
                    b->def->name,
                    b->def->variants[b->variant],
                    thread_count,
                    b->work_iterations,
                    n,
                    samples[n - 1]);

        s = summarize(samples, n);

        if (n >= MIN_RUNS && ci_width(s) <= CI_TARGET)
            break;
    }

    return s;
}

static void cpu_name(char* cpu, size_t size)
{
    snprintf(cpu, size, "unknown");
#ifdef __APPLE__
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
}

static void print_environment(FILE* out, int threads)
{
    char cpu[128];
    cpu_name(cpu, sizeof(cpu));

    struct utsname os;
    uname(&os);

    fprintf(out,
            "cpu: %s (%d hardware threads), os: %s %s %s, compiler: %s, flags: %s\n",
            cpu,
            threads,
            os.sysname,
            os.release,
            os.machine,
            __VERSION__,
            BENCH_FLAGS);
}

static FILE* csv_open(bench_t* b)
{
    char host[64] = "unknown";
    gethostname(host, sizeof(host) - 1);

    char run[32];
    const char* run_id = getenv("BENCH_RUN_ID");
    if (!run_id)
    {
        time_t now = time(NULL);
        strftime(run, sizeof(run), "%Y-%m-%d_%H-%M-%S", localtime(&now));
        run_id = run;
    }

    char path[256];
    snprintf(path, sizeof(path), BENCH_RESULTS_DIR "/%s_%s.csv", host, run_id);

    mkdir(BENCH_RESULTS_DIR, 0755);

    FILE* f = fopen(path, "a");
    if (!f)
        return NULL;

    char cpu[128];
    cpu_name(cpu, sizeof(cpu));

    struct utsname os;
    uname(&os);

    snprintf(b->csv_prefix,
             sizeof(b->csv_prefix),
             "%s,\"%s\",%ld,\"%s %s %s\",\"%s\",\"%s\",%d,%zu",
             host,
             cpu,
             sysconf(_SC_NPROCESSORS_ONLN),
             os.sysname,
             os.release,
             os.machine,
             __VERSION__,
             BENCH_FLAGS,
             b->system_count,
             b->entity_count);

    if (ftell(f) == 0)
        fputs("host,cpu,hardware_threads,os,compiler,flags,systems,entities,"
              "lib,bench,variant,threads,work_iterations,run,ms\n",
              f);

    return f;
}

static void baseline_path(char* path, size_t size, const char* lib, const bench_t* b)
{
    snprintf(path, size, "%s_%s.baseline", lib, b->def->name);
}

static bool baseline_read(baseline_t* base, const char* lib, const bench_t* b)
{
    char path[64];
    baseline_path(path, sizeof(path), lib, b);

    FILE* f = fopen(path, "r");
    if (!f)
        return false;

    int systems;
    size_t entities;
    bool ok = 3 == fscanf(f, "%d %zu %d", &systems, &entities, &base->rows) &&
              systems == b->system_count && entities == b->entity_count && base->rows > 0 &&
              base->rows <= MAX_ROWS;

    for (int r = 0; ok && r < base->rows; r++)
    {
        ok = 1 == fscanf(f, "%d", &base->threads[r]);

        for (int w = 0; ok && w < WORK_LEVEL_COUNT; w++)
            ok = 3 == fscanf(f,
                             "%lf %lf %lf",
                             &base->stats[r][w].median,
                             &base->stats[r][w].lo,
                             &base->stats[r][w].hi);
    }

    fclose(f);
    return ok;
}

static void baseline_write(const baseline_t* base, const char* lib, const bench_t* b)
{
    char path[64];
    baseline_path(path, sizeof(path), lib, b);

    FILE* f = fopen(path, "w");
    if (!f)
        return;

    fprintf(f, "%d %zu %d\n", b->system_count, b->entity_count, base->rows);

    for (int r = 0; r < base->rows; r++)
    {
        fprintf(f, "%d", base->threads[r]);

        for (int w = 0; w < WORK_LEVEL_COUNT; w++)
            fprintf(f,
                    " %f %f %f",
                    base->stats[r][w].median,
                    base->stats[r][w].lo,
                    base->stats[r][w].hi);

        fprintf(f, "\n");
    }

    fclose(f);
}

static const stat_t* baseline_row(const baseline_t* base, int threads)
{
    for (int r = 0; r < base->rows; r++)
        if (base->threads[r] == threads)
            return base->stats[r];

    return NULL;
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

static void print_speedup(const stat_t* other, stat_t s, bool one_thread)
{
    if (!other)
    {
        printf(" %10s", "");
        return;
    }

    bool overlap = s.lo <= other->hi && other->lo <= s.hi;
    printf(" %7.2fx%c%c", other->median / s.median, overlap ? '~' : ' ', one_thread ? '*' : ' ');
}

static void print_row(const bench_def_t* def,
                      int variant,
                      int threads,
                      const stat_t* stats,
                      const baseline_t* pico,
                      const baseline_t* flecs)
{

    const stat_t* pico_row  = pico ? baseline_row(pico, 1) : NULL;
    const stat_t* flecs_row = flecs ? baseline_row(flecs, threads) : NULL;
    bool flecs_one_thread   = flecs && !flecs_row;

    if (flecs_one_thread)
        flecs_row = baseline_row(flecs, 1);

    printf("%-6s %-26s %7d", def->lib, def->variants[variant], threads);

    for (int w = 0; w < WORK_LEVEL_COUNT; w++)
    {
        printf(" %10.3f +-%3.0f%%", stats[w].median, 100 * ci_width(stats[w]));
        print_speedup(pico_row ? &pico_row[w] : NULL, stats[w], false);
        print_speedup(flecs_row ? &flecs_row[w] : NULL, stats[w], flecs_one_thread);
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

    bool is_pito = strcmp(def->lib, "pito") == 0;

    baseline_t pico, flecs, own = { 0 };
    bool has_pico  = strcmp(def->lib, "pico") != 0 && baseline_read(&pico, "pico", &b);
    bool has_flecs = is_pito && baseline_read(&flecs, "flecs", &b);

    print_environment(stdout, hardware_threads);
    b.csv = csv_open(&b);
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
    printf("\n~ = CI overlaps the other library's, the difference is not significant\n");

    if (has_flecs && flecs.rows == 1)
        printf("* = compared to flecs on 1 thread, it can't run this benchmark in parallel\n");

    printf("%-6s %-26s %7s", "lib", "variant", "threads");
    for (int w = 0; w < WORK_LEVEL_COUNT; w++)
        printf(" %17s %8s  %8s  ", work_names[w], "vs pico", "vs flecs");
    printf("\n");

    int last_threads = def->serial_only ? 1 : max_threads;

    stat_t stats[WORK_LEVEL_COUNT];

    for (int v = 0; v < 2 && def->variants[v]; v++)
    {
        for (int threads = 1; threads <= last_threads; threads *= 2)
        {
            measure_row(&b, v, threads, stats);
            print_row(def, v, threads, stats, has_pico ? &pico : NULL, has_flecs ? &flecs : NULL);

            if (v == 0)
            {
                own.threads[own.rows] = threads;
                memcpy(own.stats[own.rows], stats, sizeof(stats));
                own.rows++;
            }
        }
    }

    if (!is_pito)
        baseline_write(&own, def->lib, &b);

    if (b.csv)
        fclose(b.csv);

    def->cleanup(&b);
    return 0;
}

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

#define MIN_RUNS  10
#define MAX_RUNS  100
#define CI_TARGET 0.05
#define Z_95      1.96

static const size_t work_levels[] = { 0, 10, 50, 500 };
#define WORK_LEVEL_COUNT 4

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
    const char* variants[3];
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
static void measure(bench_t* b, int thread_count)
{
    double samples[MAX_RUNS];

    b->def->run_once(b, thread_count);

    for (int n = 1; n <= MAX_RUNS; n++)
    {
        samples[n - 1] = b->def->run_once(b, thread_count);

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

        if (n >= MIN_RUNS && ci_width(summarize(samples, n)) <= CI_TARGET)
            break;
    }
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

    printf("%s: %s (%ld hardware threads), %s %s %s, %s, flags:%s\n"
           "%d systems x %zu entities, writing %s\n",
           host,
           cpu,
           sysconf(_SC_NPROCESSORS_ONLN),
           os.sysname,
           os.release,
           os.machine,
           __VERSION__,
           BENCH_FLAGS,
           b->system_count,
           b->entity_count,
           path);

    if (ftell(f) == 0)
        fputs("host,cpu,hardware_threads,os,compiler,flags,systems,entities,"
              "lib,bench,variant,threads,work_iterations,run,ms\n",
              f);

    return f;
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
    b.csv          = csv_open(&b);

    if (!b.csv)
    {
        fprintf(stderr, "%s: cannot open csv in " BENCH_RESULTS_DIR "\n", def->name);
        return 1;
    }

    int last_threads = def->serial_only ? 1 : max_threads;

    for (int v = 0; v < 3 && def->variants[v]; v++)
    {
        b.variant = v;

        for (int threads = 1; threads <= last_threads; threads *= 2)
        {
            printf("%s %s %s %d threads, work:", def->lib, def->name, def->variants[v], threads);

            for (int w = 0; w < WORK_LEVEL_COUNT; w++)
            {
                printf(" %zu", work_levels[w]);
                fflush(stdout);

                b.work_iterations = work_levels[w];
                measure(&b, threads);
            }

            printf("\n");
        }
    }

    fclose(b.csv);

    def->cleanup(&b);
    return 0;
}

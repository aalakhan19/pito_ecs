#pragma once

#include "bench.h"

#include <pthread.h>
#include <stdatomic.h>

#define PICO_ECS_IMPLEMENTATION
#define PITO_ECS_IMPLEMENTATION
#define PICO_ECS_MAX_SYSTEMS MAX_SYSTEMS
#define PITO_ECS_MAX_SYSTEMS MAX_SYSTEMS

#ifdef BENCH_PICO
#include "pico_ecs.h"
#define BENCH_LIB    "pico"
#define BENCH_SERIAL true
#else
#include "pito_ecs.h"
#define BENCH_LIB    "pito"
#define BENCH_SERIAL false
#endif

typedef struct
{
    ecs_comp_t comp;
    size_t work_iterations;
    int variant;
    size_t entity_count;
    ecs_entity_t* created;
} bench_ctx_t;

static ecs_t* ecs;
static ecs_system_t systems[MAX_SYSTEMS];
static bench_ctx_t ctx[MAX_SYSTEMS];

static void setup(bench_t* b);
static void finish(bench_t* b);

typedef struct
{
    bench_t* b;
    int thread_index;
    int thread_count;
} thread_args_t;

static atomic_int ready;
static atomic_int go;
static atomic_int done;

static void run_systems(thread_args_t* args)
{
    for (int i = args->thread_index; i < args->b->system_count; i += args->thread_count)
        ecs_run_system(ecs, systems[i], 0);
}

static void* run_thread(void* arg)
{
    atomic_fetch_add(&ready, 1);
    while (!atomic_load(&go))
        ;

    run_systems(arg);
    atomic_fetch_add(&done, 1);
    return NULL;
}

static double picto_run_once(bench_t* b, int thread_count)
{
    if (ecs)
        ecs_free(ecs);

    size_t capacity = b->entity_count * b->system_count;
#ifndef BENCH_PICO
    capacity += b->system_count * ECS_INITIALIZE_SHARD_SIZE;
#endif
    ecs = ecs_new(capacity, NULL);

    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].work_iterations = b->work_iterations;
        ctx[i].variant         = b->variant;
        ctx[i].entity_count    = b->entity_count;
    }

    setup(b);

    pthread_t threads[MAX_SYSTEMS];
    thread_args_t args[MAX_SYSTEMS];

    atomic_store(&ready, 0);
    atomic_store(&go, 0);
    atomic_store(&done, 0);

    for (int t = 0; t < thread_count; t++)
        args[t] = (thread_args_t){ b, t, thread_count };

    // main thread is thread 0
    for (int t = 1; t < thread_count; t++)
        pthread_create(&threads[t], NULL, run_thread, &args[t]);

    while (atomic_load(&ready) < thread_count - 1)
        ;

    double start = now_ms();
    atomic_store(&go, 1);

    run_systems(&args[0]);
    while (atomic_load(&done) < thread_count - 1)
        ;

    finish(b);
    double elapsed = now_ms() - start;

    for (int t = 1; t < thread_count; t++)
        pthread_join(threads[t], NULL);

    return elapsed;
}

static void picto_cleanup(bench_t* b)
{
    ecs_free(ecs);
    ecs = NULL;

    for (int i = 0; i < b->system_count; i++)
        free(ctx[i].created);
}

#ifdef BENCH_PICO
#define BENCH_PICTO_DEF(name, v0, v1)                                                              \
    { name, BENCH_LIB, { "", NULL }, BENCH_SERIAL, picto_run_once, picto_cleanup }
#else
#define BENCH_PICTO_DEF(name, v0, v1)                                                              \
    { name, BENCH_LIB, { v0, v1 }, BENCH_SERIAL, picto_run_once, picto_cleanup }
#endif

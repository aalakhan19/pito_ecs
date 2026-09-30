#pragma once

#include "bench.h"

#include "flecs.h"

typedef struct
{
    ecs_entity_t comp;
    size_t work_iterations;
    size_t entity_count;
} bench_ctx_t;

static ecs_world_t* world;
static bench_ctx_t ctx[MAX_SYSTEMS];

// Defined by each benchmark, untimed
static void setup(bench_t* b);

static ecs_entity_t bench_component(void)
{
    return ecs_component_init(world,
                              &(ecs_component_desc_t){ .type.size      = sizeof(value_t),
                                                       .type.alignment = _Alignof(value_t) });
}

static ecs_entity_t bench_system(
    ecs_iter_action_t callback, bench_ctx_t* c, bool multi_threaded, ecs_term_t t0, ecs_term_t t1)
{
    return ecs_system_init(
        world,
        &(ecs_system_desc_t){ .entity =
                                  ecs_entity(world, { .add = ecs_ids(ecs_dependson(EcsOnUpdate)) }),
                              .query.terms    = { t0, t1 },
                              .callback       = callback,
                              .ctx            = c,
                              .multi_threaded = multi_threaded });
}

static double flecs_run_once(bench_t* b, int thread_count)
{
    if (world)
        ecs_fini(world);

    world = ecs_mini();
    ECS_IMPORT(world, FlecsPipeline);

    if (thread_count > 1)
        ecs_set_threads(world, thread_count);

    ecs_dim(world, (int32_t)(b->entity_count * b->system_count));

    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].work_iterations = b->work_iterations;
        ctx[i].entity_count    = b->entity_count;
    }

    setup(b);

    double start = now_ms();
    ecs_progress(world, 0);
    return now_ms() - start;
}

static void flecs_cleanup(bench_t* b)
{
    (void)b;
    ecs_fini(world);
    world = NULL;
}

#define BENCH_FLECS_DEF(name, serial_only)                                                         \
    { name, "flecs", { "", NULL }, serial_only, flecs_run_once, flecs_cleanup }

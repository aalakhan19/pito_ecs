#include "threads.h"

#define TEST_OWNED_ATTACH_COUNT              1000
#define TEST_OWNED_ATTACH_CAPACITY           8192

#define TEST_OWNED_ATTACH_SPEEDUP_COUNT      100000
#define TEST_OWNED_ATTACH_SPEEDUP_CAPACITY   800000

typedef struct
{
    int           count;
    size_t        work_iterations;
    ecs_comp_t    comp;
    ecs_entity_t* out;
} owned_attach_ctx_t;

static ecs_ret_t owned_attach_system(ecs_t* ecs,
                                     ecs_entity_t* entities,
                                     size_t entity_count,
                                     void* udata)
{
    (void)entities;
    (void)entity_count;

    owned_attach_ctx_t* ctx = (owned_attach_ctx_t*)udata;

    for (int i = 0; i < ctx->count; i++)
    {
        ecs_entity_t entity = ecs_create_owned_sharded(ecs);

        comp_t* c = (comp_t*)ecs_add_owned(ecs, entity, ctx->comp, NULL);
        c->used = true;

        if (ctx->out)
            ctx->out[i] = entity;

        volatile double acc = 0.0;
        for (size_t j = 0; j < ctx->work_iterations; j++)
            acc += (double)j * 1.0000001;
        (void)acc;
    }

    return 0;
}


static ecs_ret_t deferred_attach_system(ecs_t* ecs,
                                        ecs_entity_t* entities,
                                        size_t entity_count,
                                        void* udata)
{
    (void)entities;
    (void) entity_count;

    owned_attach_ctx_t* ctx = (owned_attach_ctx_t*)udata;

    for (int i = 0; i < ctx->count; i++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, ctx->comp, NULL);

        if (ctx->out)
            ctx->out[i] = entity;

        volatile double acc = 0.0;
        for (size_t j = 0; j < ctx->work_iterations; j++)
            acc += (double)j * 1.0000001;
        (void)acc;
    }

    return 0;
}

TEST_CASE(test_owned_initialize_attach_parallel_correctness)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_ATTACH_CAPACITY, NULL);

    ecs_comp_t comp = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t out1[TEST_OWNED_ATTACH_COUNT];
    static ecs_entity_t out2[TEST_OWNED_ATTACH_COUNT];

    owned_attach_ctx_t ctx1 = { .count = TEST_OWNED_ATTACH_COUNT, .comp = comp, .out = out1 };
    owned_attach_ctx_t ctx2 = { .count = TEST_OWNED_ATTACH_COUNT, .comp = comp, .out = out2 };

    sys1 = ecs_define_system(ecs, owned_attach_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx1 });
    sys2 = ecs_define_system(ecs, owned_attach_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx2 });

    // Consumer system: matches every entity the two spawners produce
    ecs_system_t consumer = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, consumer, comp);

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    REQUIRE(0 == ecs_get_entity_count(ecs, consumer));

    ecs_sync_owned(ecs, out1, TEST_OWNED_ATTACH_COUNT);
    ecs_sync_owned(ecs, out2, TEST_OWNED_ATTACH_COUNT);

    REQUIRE(ecs_get_entity_count(ecs, consumer) == (size_t)(2 * TEST_OWNED_ATTACH_COUNT));

    // The spawners themselves require nothing, so they must stay empty
    REQUIRE(0 == ecs_get_entity_count(ecs, sys1));
    REQUIRE(0 == ecs_get_entity_count(ecs, sys2));

    static bool seen[TEST_OWNED_ATTACH_CAPACITY];
    memset(seen, 0, sizeof(seen));

    ecs_entity_t* outs[2] = { out1, out2 };

    for (int o = 0; o < 2; o++)
    {
        for (int i = 0; i < TEST_OWNED_ATTACH_COUNT; i++)
        {
            ecs_entity_t entity = outs[o][i];

            REQUIRE(entity.id < (ecs_id_t)TEST_OWNED_ATTACH_CAPACITY);
            REQUIRE(ecs_is_ready(ecs, entity));

            REQUIRE(ecs_has(ecs, entity, comp));

            comp_t* c = (comp_t*)ecs_get(ecs, entity, comp);
            REQUIRE(c->used);

            REQUIRE(!seen[entity.id]);
            seen[entity.id] = true;
        }
    }

    return true;
}

TEST_CASE(test_owned_initialize_attach_parallel_speedup)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_ATTACH_SPEEDUP_CAPACITY, NULL);

    ecs_comp_t comp = ecs_define_component(ecs, sizeof(comp_t), NULL);

    size_t bytes = TEST_OWNED_ATTACH_SPEEDUP_COUNT * sizeof(ecs_entity_t);
    ecs_entity_t* out1 = (ecs_entity_t*)malloc(bytes);
    ecs_entity_t* out2 = (ecs_entity_t*)malloc(bytes);

    REQUIRE(NULL != out1 && NULL != out2);

    owned_attach_ctx_t ctx1 = { .count = TEST_OWNED_ATTACH_SPEEDUP_COUNT, .comp = comp, .out = out1 };
    owned_attach_ctx_t ctx2 = { .count = TEST_OWNED_ATTACH_SPEEDUP_COUNT, .comp = comp, .out = out2 };

    sys1 = ecs_define_system(ecs, owned_attach_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx1 });
    sys2 = ecs_define_system(ecs, owned_attach_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx2 });

    ecs_system_t def1 = ecs_define_system(ecs, deferred_attach_system,
                                          &(ecs_sys_desc_t){ .udata = &ctx1 });
    ecs_system_t def2 = ecs_define_system(ecs, deferred_attach_system,
                                          &(ecs_sys_desc_t){ .udata = &ctx2 });

    ecs_system_t consumer = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, consumer, comp);

    struct timespec t0, t1, t2, t3, t4, t5, t6;

    // normal run
    timespec_get(&t0, TIME_UTC);
    ecs_run_system(ecs, def1, 0);
    ecs_run_system(ecs, def2, 0);
    timespec_get(&t1, TIME_UTC);

    REQUIRE(ecs_get_entity_count(ecs, consumer) ==
            (size_t)(2 * TEST_OWNED_ATTACH_SPEEDUP_COUNT));

    // owned but still one thread
    timespec_get(&t1, TIME_UTC);
    ecs_run_system(ecs, sys1, 0);
    ecs_run_system(ecs, sys2, 0);
    timespec_get(&t2, TIME_UTC);
    ecs_sync_owned(ecs, out1, TEST_OWNED_ATTACH_SPEEDUP_COUNT);
    ecs_sync_owned(ecs, out2, TEST_OWNED_ATTACH_SPEEDUP_COUNT);
    timespec_get(&t3, TIME_UTC);

    // owned with seperate thread
    test_thread_t th1, th2;

    timespec_get(&t4, TIME_UTC);
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&th1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&th2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(th1);
    TEST_THREAD_JOIN(th2);
    timespec_get(&t5, TIME_UTC);

    ecs_sync_owned(ecs, out1, TEST_OWNED_ATTACH_SPEEDUP_COUNT);
    ecs_sync_owned(ecs, out2, TEST_OWNED_ATTACH_SPEEDUP_COUNT);
    timespec_get(&t6, TIME_UTC);

    REQUIRE(ecs_get_entity_count(ecs, consumer) == (size_t)(6 * TEST_OWNED_ATTACH_SPEEDUP_COUNT));

    double deferred_s = test_elapsed_seconds(t0, t1);

    double serial_spawn_s   = test_elapsed_seconds(t1, t2);
    double serial_sync_s    = test_elapsed_seconds(t2, t3);
    double parallel_spawn_s = test_elapsed_seconds(t4, t5);
    double parallel_sync_s  = test_elapsed_seconds(t5, t6);

    double serial_s   = serial_spawn_s + serial_sync_s;
    double parallel_s = parallel_spawn_s + parallel_sync_s;

    printf("owned_initialize+attach, %d entities x 2 systems:\n"
           "    deferred = %.4fs\n"
           "    owned    (1 thread)  spawn=%.4fs sync=%.4fs total=%.4fs (%.2fx vs deferred)\n"
           "    owned    (2 threads) spawn=%.4fs sync=%.4fs total=%.4fs (%.2fx vs deferred, %.2fx vs owned serial)\n",
           TEST_OWNED_ATTACH_SPEEDUP_COUNT,
           deferred_s,
           serial_spawn_s, serial_sync_s, serial_s,
           serial_s > 0.0 ? deferred_s / serial_s : 0.0,
           parallel_spawn_s, parallel_sync_s, parallel_s,
           parallel_s > 0.0 ? deferred_s / parallel_s : 0.0,
           parallel_s > 0.0 ? serial_s / parallel_s : 0.0);

    free(out1);
    free(out2);

    return true;
}

TEST_SUITE(suite_owned_attach)
{
    RUN_TEST_CASE(test_owned_initialize_attach_parallel_correctness);
    RUN_TEST_CASE(test_owned_initialize_attach_parallel_speedup);
}

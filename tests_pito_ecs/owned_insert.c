#include "threads.h"

#define TEST_OWNED_INSERT_COUNT              4000
#define TEST_OWNED_INSERT_CAPACITY           16384

#define TEST_OWNED_INSERT_SPEEDUP_COUNT      200000
#define TEST_OWNED_INSERT_SPEEDUP_CAPACITY   400000
#define TEST_OWNED_INSERT_SPEEDUP_WORK_ITERS 500

typedef struct
{
    ecs_comp_t comp;
    size_t     work_iterations;
} owned_insert_ctx_t;

static ecs_ret_t owned_insert_system(ecs_t* ecs,
                                     ecs_entity_t* entities,
                                     size_t entity_count,
                                     void* udata)
{
    owned_insert_ctx_t* ctx = (owned_insert_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        volatile double acc = 0.0;
        for (size_t j = 0; j < ctx->work_iterations; j++)
            acc += (double)j * 1.0000001;
        (void)acc;

        ecs_insert_owned(ecs, entities[i], ctx->comp, NULL);
    }

    return 0;
}

static ecs_ret_t deferred_insert_system(ecs_t* ecs,
                                        ecs_entity_t* entities,
                                        size_t entity_count,
                                        void* udata)
{
    owned_insert_ctx_t* ctx = (owned_insert_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        volatile double acc = 0.0;
        for (size_t j = 0; j < ctx->work_iterations; j++)
            acc += (double)j * 1.0000001;
        (void)acc;

        ecs_add(ecs, entities[i], ctx->comp, NULL);
    }

    return 0;
}

TEST_CASE(test_owned_insert_parallel_correctness)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_INSERT_CAPACITY, NULL);

    comp1 = ecs_define_component(ecs, sizeof(comp_t), NULL);
    comp2 = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t entities[TEST_OWNED_INSERT_COUNT];

    owned_insert_ctx_t ctx = { .comp = comp2, .work_iterations = 0 };

    sys1 = ecs_define_system(ecs, owned_insert_system,
                             &(ecs_sys_desc_t){ .owned_insert = true, .udata = &ctx });
    ecs_require(ecs, sys1, comp1);
    ecs_exclude(ecs, sys1, comp2);

    for (int i = 0; i < TEST_OWNED_INSERT_COUNT; i++)
    {
        entities[i] = ecs_create(ecs);
        ecs_add(ecs, entities[i], comp1, NULL);
    }

    REQUIRE(TEST_OWNED_INSERT_COUNT == (int)ecs_get_entity_count(ecs, sys1));

    ecs_run_system(ecs, sys1, 0);

    for (int i = 0; i < TEST_OWNED_INSERT_COUNT; i++)
        REQUIRE(ecs_has(ecs, entities[i], comp2));

#if !PITO_ECS_OWNED_DELETE_AUTO_FLUSH
    ecs_sync_owned_delete(ecs);
#endif

    REQUIRE(0 == (int)ecs_get_entity_count(ecs, sys1));

    ecs_run_system(ecs, sys1, 0);

#if !PITO_ECS_OWNED_DELETE_AUTO_FLUSH
    ecs_sync_owned_delete(ecs);
#endif

    REQUIRE(0 == (int)ecs_get_entity_count(ecs, sys1));

    return true;
}

TEST_CASE(test_owned_insert_join_is_deferred)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_INSERT_CAPACITY, NULL);

    comp1 = ecs_define_component(ecs, sizeof(comp_t), NULL);
    comp2 = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t entities[TEST_OWNED_INSERT_COUNT];

    owned_insert_ctx_t ctx = { .comp = comp2, .work_iterations = 0 };

    sys1 = ecs_define_system(ecs, owned_insert_system,
                             &(ecs_sys_desc_t){ .owned_insert = true, .udata = &ctx });
    ecs_require(ecs, sys1, comp1);
    ecs_exclude(ecs, sys1, comp2);

    sys2 = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, sys2, comp1);
    ecs_require(ecs, sys2, comp2);

    for (int i = 0; i < TEST_OWNED_INSERT_COUNT; i++)
    {
        entities[i] = ecs_create(ecs);
        ecs_add(ecs, entities[i], comp1, NULL);

        if (0 == i % 2)
            ecs_add(ecs, entities[i], comp2, NULL);
    }

    REQUIRE(TEST_OWNED_INSERT_COUNT / 2 == (int)ecs_get_entity_count(ecs, sys2));

    ecs_run_system(ecs, sys1, 0);

    REQUIRE(ecs_has(ecs, entities[1], comp2));

#if PITO_ECS_OWNED_DELETE_AUTO_FLUSH
    REQUIRE(TEST_OWNED_INSERT_COUNT == (int)ecs_get_entity_count(ecs, sys2));
#else
    REQUIRE(TEST_OWNED_INSERT_COUNT / 2 == (int)ecs_get_entity_count(ecs, sys2));
#endif

    ecs_sync_owned_delete(ecs);

    REQUIRE(0 == (int)ecs_get_entity_count(ecs, sys1));
    REQUIRE(TEST_OWNED_INSERT_COUNT == (int)ecs_get_entity_count(ecs, sys2));

    return true;
}

TEST_CASE(test_owned_insert_parallel_disjoint_components)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_INSERT_CAPACITY, NULL);

    comp1 = ecs_define_component(ecs, sizeof(comp_t), NULL);
    comp2 = ecs_define_component(ecs, sizeof(comp_t), NULL);
    comp3 = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t entities[TEST_OWNED_INSERT_COUNT];

    owned_insert_ctx_t ctx1 = { .comp = comp2, .work_iterations = 0 };
    owned_insert_ctx_t ctx2 = { .comp = comp3, .work_iterations = 0 };

    sys1 = ecs_define_system(ecs, owned_insert_system,
                             &(ecs_sys_desc_t){ .owned_insert = true, .udata = &ctx1 });
    ecs_require(ecs, sys1, comp1);
    ecs_exclude(ecs, sys1, comp2);

    sys2 = ecs_define_system(ecs, owned_insert_system,
                             &(ecs_sys_desc_t){ .owned_insert = true, .udata = &ctx2 });
    ecs_require(ecs, sys2, comp1);
    ecs_exclude(ecs, sys2, comp3);

    for (int i = 0; i < TEST_OWNED_INSERT_COUNT; i++)
    {
        entities[i] = ecs_create(ecs);
        ecs_add(ecs, entities[i], comp1, NULL);
    }

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    ecs_sync_owned_delete(ecs);

    REQUIRE(0 == ecs_get_entity_count(ecs, sys1));
    REQUIRE(0 == ecs_get_entity_count(ecs, sys2));

    for (int i = 0; i < TEST_OWNED_INSERT_COUNT; i++)
    {
        REQUIRE(ecs_has(ecs, entities[i], comp2));
        REQUIRE(ecs_has(ecs, entities[i], comp3));
    }

    return true;
}

static void owned_insert_speedup_setup(ecs_system_fn cb, bool owned, owned_insert_ctx_t* ctx_a, owned_insert_ctx_t* ctx_b, ecs_system_t* sys_a, ecs_system_t* sys_b)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_INSERT_SPEEDUP_CAPACITY, NULL);

    ecs_comp_t base_comp = ecs_define_component(ecs, sizeof(comp_t), NULL);

    ctx_a->comp = ecs_define_component(ecs, sizeof(comp_t), NULL);
    ctx_b->comp = ecs_define_component(ecs, sizeof(comp_t), NULL);

    *sys_a = ecs_define_system(ecs, cb, &(ecs_sys_desc_t){ .owned_insert = owned, .udata = ctx_a });
    *sys_b = ecs_define_system(ecs, cb, &(ecs_sys_desc_t){ .owned_insert = owned, .udata = ctx_b });

    ecs_require(ecs, *sys_a, base_comp);
    ecs_exclude(ecs, *sys_a, ctx_a->comp);

    ecs_require(ecs, *sys_b, base_comp);
    ecs_exclude(ecs, *sys_b, ctx_b->comp);

    for (int i = 0; i < TEST_OWNED_INSERT_SPEEDUP_COUNT; i++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, base_comp, NULL);
    }
}

static bool owned_insert_speedup_run(size_t work_iterations)
{
    owned_insert_ctx_t ctx_a = { .work_iterations = work_iterations };
    owned_insert_ctx_t ctx_b = { .work_iterations = work_iterations };
    ecs_system_t def1, def2;

    // baseline
    owned_insert_speedup_setup(deferred_insert_system, false, &ctx_a, &ctx_b, &def1, &def2);

    struct timespec t0, t1;

    timespec_get(&t0, TIME_UTC);
    ecs_run_system(ecs, def1, 0);
    ecs_run_system(ecs, def2, 0);
    timespec_get(&t1, TIME_UTC);

    double deferred_s = test_elapsed_seconds(t0, t1);

    // owned one thread
    owned_insert_speedup_setup(owned_insert_system, true, &ctx_a, &ctx_b, &sys1, &sys2);

    struct timespec t2, t3, t4;

    timespec_get(&t2, TIME_UTC);
    ecs_run_system(ecs, sys1, 0);
    ecs_run_system(ecs, sys2, 0);
    timespec_get(&t3, TIME_UTC);
    ecs_sync_owned_delete(ecs);
    timespec_get(&t4, TIME_UTC);

    double serial_insert_s = test_elapsed_seconds(t2, t3);
    double serial_sync_s   = test_elapsed_seconds(t3, t4);

    // owned 2 threads
    owned_insert_speedup_setup(owned_insert_system, true, &ctx_a, &ctx_b, &sys1, &sys2);

    test_thread_t th1, th2;
    struct timespec t5, t6, t7;

    timespec_get(&t5, TIME_UTC);
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&th1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&th2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(th1);
    TEST_THREAD_JOIN(th2);
    timespec_get(&t6, TIME_UTC);
    ecs_sync_owned_delete(ecs);
    timespec_get(&t7, TIME_UTC);

    double parallel_insert_s = test_elapsed_seconds(t5, t6);
    double parallel_sync_s   = test_elapsed_seconds(t6, t7);

    double serial_s   = serial_insert_s + serial_sync_s;
    double parallel_s = parallel_insert_s + parallel_sync_s;

    printf("owned_insert, %d entities x 2 systems, work_iterations=%zu:\n"
           "    deferred = %.4fs\n"
           "    owned    (1 thread)  insert=%.4fs sync=%.4fs total=%.4fs (%.2fx vs deferred)\n"
           "    owned    (2 threads) insert=%.4fs sync=%.4fs total=%.4fs (%.2fx vs deferred, %.2fx vs owned serial)\n",
           TEST_OWNED_INSERT_SPEEDUP_COUNT, work_iterations,
           deferred_s,
           serial_insert_s, serial_sync_s, serial_s,
           serial_s > 0.0 ? deferred_s / serial_s : 0.0,
           parallel_insert_s, parallel_sync_s, parallel_s,
           parallel_s > 0.0 ? deferred_s / parallel_s : 0.0,
           parallel_s > 0.0 ? serial_s / parallel_s : 0.0);

    return true;
}

TEST_CASE(test_owned_insert_parallel_speedup_no_work)
{
    return owned_insert_speedup_run(0);
}

TEST_CASE(test_owned_insert_parallel_speedup)
{
    return owned_insert_speedup_run(TEST_OWNED_INSERT_SPEEDUP_WORK_ITERS);
}

TEST_SUITE(suite_owned_insert)
{
    RUN_TEST_CASE(test_owned_insert_parallel_correctness);
    RUN_TEST_CASE(test_owned_insert_join_is_deferred);
    RUN_TEST_CASE(test_owned_insert_parallel_disjoint_components);
    RUN_TEST_CASE(test_owned_insert_parallel_speedup_no_work);
    RUN_TEST_CASE(test_owned_insert_parallel_speedup);
}
